#include "HPNL/ExternalEqService.h"

ExternalEqService::ExternalEqService(const char* ip_, const char* port_, bool is_server_) : ip(ip_), port(port_), is_server(is_server_) {
  stack = new FIStack(ip, port, is_server ? FI_SOURCE : 0);
  eq_demulti_plexer = new EQExternalDemultiplexer(stack);
  recvBufMgr = new ExternalEqServiceBufMgr();
  sendBufMgr = new ExternalEqServiceBufMgr();
}

ExternalEqService::~ExternalEqService() {
  delete stack;
  std::cout << "external eq service deleted" << std::endl;
  delete eq_demulti_plexer;
  delete recvBufMgr;
  delete sendBufMgr;
}

fid_eq* ExternalEqService::accept(fi_info* info) {
  HandlePtr eqHandle;
  eqHandle = stack->accept(info, recvBufMgr, sendBufMgr);
  return (fid_eq*)eqHandle->get_ctx();
}

fid_eq* ExternalEqService::connect() {
  HandlePtr eqHandle;
  if (is_server) {
    eqHandle = stack->bind();
    stack->listen();
  } else {
    eqHandle = stack->connect(recvBufMgr, sendBufMgr);  
  }
  return (fid_eq*)eqHandle->get_ctx();
}

void ExternalEqService::set_recv_buffer(char* buffer, uint64_t size, int rdma_buffer_id) {
  Chunk *ck = new Chunk();
  ck->buffer = buffer;
  ck->rdma_buffer_id = rdma_buffer_id;
  ck->capacity = size;
  recvBufMgr->add(ck->rdma_buffer_id, ck);
}

void ExternalEqService::set_send_buffer(char* buffer, uint64_t size, int rdma_buffer_id) {
  Chunk *ck = new Chunk();
  ck->buffer = buffer;
  ck->rdma_buffer_id = rdma_buffer_id;
  ck->capacity = size;
  sendBufMgr->add(ck->rdma_buffer_id, ck);
}

int ExternalEqService::wait_eq_event(fi_info** info, fid_eq** eq) {
  int ret = eq_demulti_plexer->wait_event(info, eq);
  return ret;
}

Connection* ExternalEqService::get_connection(fid_eq* eq) {
  FIConnection *con = stack->get_connection(&eq->fid);
  return con;
}

void ExternalEqService::reap(fid *con_id) {
  stack->reap(con_id);
}

FIStack* ExternalEqService::get_stack() {
  return stack;
}

Chunk* ExternalEqService::get_chunk(int id, int type) {
  Chunk *ck = NULL;
  if (type == RECV_CHUNK)
    ck = recvBufMgr->index(id);
  else
    ck = sendBufMgr->index(id);
  return ck;
}

int ExternalEqService::add_eq_event(fid_eq *eq) {
  eq_demulti_plexer->add_event(eq);
  return 0;
}

int ExternalEqService::delete_eq_event(fid_eq *eq) {
  assert(eq_demulti_plexer);
  eq_demulti_plexer->delete_event(eq);
  return 0;
}
