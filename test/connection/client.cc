#include "HPNL/Connection.h"
#include "HPNL/Client.h"
#include "HPNL/BufMgr.h"
#include "HPNL/Callback.h"
#include "HPNL/Common.h"
#include "ConBufMgr.h"

#define SIZE 4096

uint64_t start, end = 0;

uint64_t timestamp_now() {
  return std::chrono::high_resolution_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
}

class ShutdownCallback : public Callback {
  public:
    ShutdownCallback(Client *_clt) : clt(_clt) {}
    virtual ~ShutdownCallback() {}
    virtual void operator()(void *param_1, void *param_2) override {
      std::cout << "connection shutdown..." << std::endl;
      //clt->shutdown();
    }
  private:
    Client *clt;
};

class ConnectedCallback : public Callback {
  public:
    ConnectedCallback(BufMgr *bufMgr_) : bufMgr(bufMgr_) {}
    virtual ~ConnectedCallback() {}
    virtual void operator()(void *param_1, void *param_2) override {
      Connection *con = (Connection*)param_1;
      con->shutdown();
    }
  private:
    BufMgr *bufMgr;
};

void connect() {
  BufMgr *recvBufMgr = new ConBufMgr();
  Chunk *ck;
  for (int i = 0; i < MEM_SIZE; i++) {
    ck = new Chunk();
    ck->rdma_buffer_id = recvBufMgr->get_id();
    ck->buffer = std::malloc(BUFFER_SIZE);
    recvBufMgr->add(ck->rdma_buffer_id, ck);
  }
  BufMgr *sendBufMgr = new ConBufMgr();
  for (int i = 0; i < MEM_SIZE; i++) {
    ck = new Chunk();
    ck->rdma_buffer_id = sendBufMgr->get_id();
    ck->buffer = std::malloc(BUFFER_SIZE);
    sendBufMgr->add(ck->rdma_buffer_id, ck);
  }
  Client *client = new Client("172.168.2.106", "123456");
  client->set_recv_buf_mgr(recvBufMgr);
  client->set_send_buf_mgr(sendBufMgr);

  ConnectedCallback *connectedCallback = new ConnectedCallback(sendBufMgr);
  ShutdownCallback *shutdownCallback = new ShutdownCallback(client);

  client->set_recv_callback(NULL);
  client->set_send_callback(NULL);
  client->set_connected_callback(connectedCallback);
  client->set_shutdown_callback(shutdownCallback);

  client->run(200);

  client->wait();

  delete shutdownCallback;
  delete connectedCallback;
  delete client;

  int recv_chunk_size = recvBufMgr->get_id();
  assert(recv_chunk_size == MEM_SIZE);
  for (int i = 0; i < recv_chunk_size; i++) {
    Chunk *ck = recvBufMgr->index(i);
    free(ck->buffer);
  }
  int send_chunk_size = sendBufMgr->get_id();
  for (int i = 0; i < send_chunk_size; i++) {
    Chunk *ck = sendBufMgr->index(i);
    free(ck->buffer);
  }

  delete recvBufMgr;
  delete sendBufMgr;

  shutdownCallback = NULL;
  connectedCallback = NULL;
  client = NULL;
  recvBufMgr = NULL;
  sendBufMgr = NULL;
}

int main(int argc, char *argv[]) {
  connect();  
  return 0;
}