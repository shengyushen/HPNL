#include <string.h>

#include "HPNL/Connection.h"
#include "HPNL/Client.h"
#include "HPNL/ChunkMgr.h"
#include "HPNL/Callback.h"

#include <iostream>

#define MSG_SIZE 4096
#define BUFFER_SIZE 65536
#define BUFFER_NUM 65536

int count = 0;
uint64_t start, end = 0;
std::mutex mtx;

uint64_t timestamp_now() {
  return std::chrono::high_resolution_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
}

class ShutdownCallback : public Callback {
  public:
    explicit ShutdownCallback(Client *_clt) : clt(_clt) {}
    ~ShutdownCallback() override = default;
    void operator()(void *param_1, void *param_2) override {
      std::cout << "connection shutdown..." << std::endl;
      clt->shutdown();
    }
  private:
    Client *clt;
};

class ConnectedCallback : public Callback {
  public:
    explicit ConnectedCallback(ChunkMgr *bufMgr_) : bufMgr(bufMgr_) {}
    ~ConnectedCallback() override = default;
    void operator()(void *param_1, void *param_2) override {
      auto con = (Connection*)param_1;
      char* buffer = (char*)std::malloc(MSG_SIZE);
      memset(buffer, '0', MSG_SIZE);
      con->sendBuf(buffer, MSG_SIZE);
      std::free(buffer);
    }
  private:
    ChunkMgr *bufMgr;
};

class RecvCallback : public Callback {
  public:
    RecvCallback(Client *client_, ChunkMgr *bufMgr_) : client(client_), bufMgr(bufMgr_) {}
    ~RecvCallback() override = default;
    void operator()(void *param_1, void *param_2) override {
      std::lock_guard<std::mutex> lk(mtx);
      count++;
      int mid = *(int*)param_1;
      Chunk *ck = bufMgr->get(mid);
      auto con = (Connection*)ck->con;
      if (count >= 1000000) {
        end = timestamp_now();
        printf("finished, totally consumes %f s, message round trip time is %f us.\n", (end-start)/1000.0, (end-start)*1000/1000000.0);
        return;
      }
      if (count == 1) {
        printf("start ping-pong.\n");
      }
      if (count == 1) {
        start = timestamp_now(); 
      }
      con->sendBuf((char*)ck->buffer, MSG_SIZE);
    }
  private:
    Client *client;
    ChunkMgr *bufMgr;
};

class SendCallback : public Callback {
  public:
    explicit SendCallback(ChunkMgr *bufMgr_) : bufMgr(bufMgr_) {}
    ~SendCallback() override = default;
    void operator()(void *param_1, void *param_2) override;
  private:
    ChunkMgr *bufMgr;
};

void SendCallback::operator()(void *param_1, void *) {
  int mid = *(int*)param_1;
  Chunk *ck = bufMgr->get(mid);
  auto con = (Connection*)ck->con;
  con->activate_send_chunk(ck);
}

int main(int argc, char *argv[]) {
  ChunkMgr *bufMgr = new DefaultChunkMgr(BUFFER_NUM, BUFFER_SIZE);

  auto client = new Client(1, 16);
  client->init();
  client->set_buf_mgr(bufMgr);

  auto recvCallback = new RecvCallback(client, bufMgr);
  auto sendCallback = new SendCallback(bufMgr);
  auto connectedCallback = new ConnectedCallback(bufMgr);
  auto shutdownCallback = new ShutdownCallback(client);

  client->set_recv_callback(recvCallback);
  client->set_send_callback(sendCallback);
  client->set_connected_callback(connectedCallback);
  client->set_shutdown_callback(shutdownCallback);

  client->start();
  client->connect("172.168.2.106", "12345");

  client->wait();

  delete shutdownCallback;
  delete connectedCallback;
  delete sendCallback;
  delete recvCallback;
  delete client;
  delete bufMgr;
  return 0;
}
