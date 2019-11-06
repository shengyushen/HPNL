package com.intel.hpnl.api;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Queue;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentLinkedQueue;

import com.intel.hpnl.core.FixedSizeBufferAllocator;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public abstract class AbstractConnection implements Connection {
  protected HpnlService service;
  protected HpnlBufferAllocator allocator;
  private Handler connectedCallback = null;
  private Handler recvCallback = null;
  private Handler sendCallback = null;
  private Handler readCallback = null;
  private List<Handler> shutdownCallbacks = new ArrayList();
  private Handler generalCallback;
  private Map<Integer, HpnlBuffer> sendBufferMap;
  private Queue<HpnlBuffer> sendBufferList;
  private Map<Integer, HpnlBuffer> recvBufferMap;
  private String destAddr;
  private int destPort;
  private String srcAddr;
  private int srcPort;
  private boolean connected;
  protected int cqIndex;
  protected long connectId;
  private static final Logger log = LoggerFactory.getLogger(AbstractConnection.class);

  protected AbstractConnection(long nativeCon, HpnlService service, long connectId) {
    this.service = service;
    this.sendBufferMap = new ConcurrentHashMap<>();
    this.recvBufferMap = new ConcurrentHashMap();
    this.sendBufferList = new ConcurrentLinkedQueue<>();
    this.initialize(nativeCon);
    this.connected = true;
    if (connectId == 0L) {
      this.connectId = service.getNewConnectionId();
    } else {
      this.connectId = connectId;
    }

    this.shutdownCallbacks.add(new AbstractConnection.InternalShutdownCallback());
  }

  @Override
  public void setHpnlBufferAllocator(HpnlBufferAllocator allocator){
      this.allocator = allocator;
  }

  @Override
  public int getCqIndex() {
    return this.cqIndex;
  }

  public void setCqIndex(int cqIndex){
    this.cqIndex = cqIndex;
  }

  @Override
  public long getConnectionId() {
    return this.connectId;
  }

  protected abstract void initialize(long nativeHandle);

  protected abstract long getNativeHandle();

  protected abstract boolean isServer();

  public Handler getConnectedCallback() {
    return this.connectedCallback;
  }

  public Handler getRecvCallback() {
    return this.recvCallback;
  }

  @Override
  public void setRecvCallback(Handler callback) {
    this.recvCallback = callback;
  }

  public Handler getSendCallback() {
    return this.sendCallback;
  }

  @Override
  public void setConnectedCallback(Handler callback){
    this.connectedCallback = callback;
  }

  public void setSendCallback(Handler callback) {
    this.sendCallback = callback;
  }

  @Override
  public void setReadCallback(Handler callback) {
    this.readCallback = callback;
  }

  public List<Handler> getShutdownCallbacks() {
    return this.shutdownCallbacks;
  }

  @Override
  public void addShutdownCallback(Handler callback) {
    this.shutdownCallbacks.add(callback);
  }

  public void setGeneralEventCallback(Handler callback) {
    this.generalCallback = callback;
  }

  public Handler getGeneralCallback() {
    return this.generalCallback;
  }

  public void reclaimSendBuffer(int bufferId, int ctxId) {
//    log.info("reclaim send buffer: {}, {}", bufferId, ctxId);
    if(bufferId < 0) {
      reclaimGlobalBuffer(bufferId, ctxId);
      return;
    }
    //no sync since buffer id is unique
    //send buffer
    HpnlBuffer buffer = this.sendBufferMap.get(Integer.valueOf(bufferId));
    if (buffer == null) {
      throw new IllegalStateException("failed to reclaim send buffer (not found) with id: " + bufferId);
    }
    this.sendBufferList.offer(buffer);
  }

  protected void reclaimGlobalBuffer(int bufferId, int ctxId){}

  @Override
  public void pushSendBuffer(HpnlBuffer buffer) {
    if(buffer.getBufferId() <= 0){
      throw new IllegalStateException("buffer id should be greater than 0. "+buffer.getBufferId());
    }
    this.sendBufferMap.put(buffer.getBufferId(), buffer);
    this.sendBufferList.offer(buffer);
  }

  @Override
  public void pushRecvBuffer(HpnlBuffer buffer) {
    if(buffer.getBufferId() <= 0) {
      throw new IllegalStateException("buffer id should be greater than 0. " + buffer.getBufferId());
    }
    this.recvBufferMap.put(buffer.getBufferId(), buffer);
  }

  @Override
  public HpnlBuffer takeSendBuffer() {
    HpnlBuffer buffer = this.sendBufferList.poll();
    if(buffer == null){
      if(log.isDebugEnabled()){
        log.debug("Connection ({}) lack of send buffer", this.getConnectionId());
      }
      return null;
    }
    buffer.clear();
    return buffer;
  }

  @Override
  public HpnlBuffer getSendBuffer(int bufferId) {
    return this.sendBufferMap.get(bufferId);
  }

  @Override
  public HpnlBuffer getRecvBuffer(int bufferId) {
    return this.recvBufferMap.get(bufferId);
  }

  /**
   * it's called from single thread of event task
   * @param eventType
   * @param bufferId
   * @param bufferSize
   * @return
   * @throws InterruptedException
   */
  protected int handleCallback(int eventType, int bufferId, int bufferSize){
    int e;
    if(log.isTraceEnabled()){
      log.trace("handle callback with event: "+eventType);
    }
    switch(eventType) {
      case EventType.RECV_EVENT:
        HpnlBuffer buffer = getRecvBuffer(bufferId);
        buffer.received();
        buffer.parse(bufferSize);
        return this.safeExecuteCallback(this.recvCallback, buffer);
      case EventType.SEND_EVENT:
        e = this.safeExecuteCallback(this.sendCallback, bufferId, bufferSize);
        if(e == Handler.RESULT_DEFAULT){
          this.reclaimSendBuffer(bufferId, bufferSize);
        }
        return e;
      case EventType
              .CONNECTED_EVENT:
        return this.safeExecuteCallback(this.connectedCallback, bufferId, 0);
      case EventType.READ_EVENT:
        return this.safeExecuteCallback(this.readCallback, bufferId, bufferSize);
      case EventType.SHUTDOWN:
        return this.executeCallbacks(this.shutdownCallbacks, bufferId, 0);
    }
    return Handler.RESULT_DEFAULT;
  }

  protected int safeExecuteCallback(Handler handler, HpnlBuffer buffer){
    if (handler == null) {
      if(log.isTraceEnabled()){
        log.trace("no recv callback set, skip recv callback");
      }
      return Handler.RESULT_DEFAULT;
    }
    try {
      return handler.handle(this, buffer);
    } catch (Throwable e) {
      log.error("failed to execute callback " + handler, e);
      return Handler.RESULT_DEFAULT;
    }
  }

  private int safeExecuteCallback(Handler handler, int bufferId, int bufferSize) {
    if (handler == null) {
      return Handler.RESULT_DEFAULT;
    }
    try {
      return handler.handle(this, bufferId, bufferSize);
    } catch (Throwable e) {
      log.error("failed to execute callback " + handler, e);
      return Handler.RESULT_DEFAULT;
    }
  }

  private int executeCallbacks(List<Handler> callbacks, int bufferId, int bufferSize) {
    int ret = Handler.RESULT_DEFAULT;
    Iterator<Handler> it = callbacks.iterator();

    while(it.hasNext()) {
      Handler callback = it.next();
      int tmp = this.safeExecuteCallback(callback, bufferId, bufferSize);
      if (tmp == 0) {
        ret = tmp;
      }
    }

    return ret;
  }

  public void setAddrInfo(String destAddr, int destPort, String srcAddr, int srcPort) {
    this.destAddr = destAddr;
    this.destPort = destPort;
    this.srcAddr = srcAddr;
    this.srcPort = srcPort;
  }

  @Override
  public String getDestAddr() {
    return this.destAddr;
  }

  @Override
  public int getDestPort() {
    return this.destPort;
  }

  @Override
  public String getSrcAddr() {
    return this.srcAddr;
  }

  @Override
  public int getSrcPort() {
    return this.srcPort;
  }

  @Override
  public ByteBuffer getLocalName() {
    throw new UnsupportedOperationException();
  }

  @Override
  public void shutdown() {
    this.shutdown(true);
  }

  private void shutdown(boolean proactive) {
    if (this.connected) {
      synchronized(this) {
        if (this.connected) {
          this.doShutdown(proactive);
          this.connected = false;
          if (log.isDebugEnabled()) {
            log.debug("connection {} with CQ index {} closed.", this.connectId, this.cqIndex);
          }
        }
      }
    }
  }

  protected abstract void doShutdown(boolean proactive);

  private class InternalShutdownCallback implements Handler {
    private InternalShutdownCallback() {
    }

    public int handle(Connection connection, int bufferId, int bufferSize) {
      AbstractConnection.this.shutdown(false);
      return Handler.RESULT_DEFAULT;
    }
  }
}
