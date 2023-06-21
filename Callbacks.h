#pragma once 

#include <functional>
#include <memory>

class Timestamp;
class Buffer;
class TCPConnection;

using TCPConnectionPtr = std::shared_ptr<TCPConnection>;
using ConnectionCallback = std::function<void(const TCPConnectionPtr&)>;
using CloseCallback = std::function<void(const TCPConnectionPtr&)>;
using WriteCompleteCallback = std::function<void(const TCPConnectionPtr&)>;
using HighWaterMarkCallback = std::function<void(TCPConnectionPtr&, size_t)>;

using MessageCallback = std::function<void(const TCPConnectionPtr&, Buffer*, Timestamp)>;