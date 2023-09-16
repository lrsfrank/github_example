// This autogenerated skeleton file illustrates how to build a server.
// You should copy it to another filename to avoid overwriting it.

#include <thrift/concurrency/ThreadManager.h>
#include <thrift/concurrency/ThreadFactory.h>
#include <thrift/server/TThreadPoolServer.h>
#include <thrift/server/TThreadPoolServer.h>
#include <thrift/server/TThreadedServer.h>
#include <thrift/TToString.h>
#include "match_server/Match.h"
#include "save_client/Save.h"
#include <thrift/protocol/TBinaryProtocol.h>
#include <thrift/server/TSimpleServer.h>
#include <thrift/transport/TServerSocket.h>
#include <thrift/transport/TBufferTransports.h>
#include <thrift/transport/TSocket.h>
#include <thrift/transport/TTransportUtils.h>
#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <unistd.h>
#include <stdexcept>
#include <sstream>

using namespace ::apache::thrift;
using namespace ::apache::thrift::protocol;
using namespace ::apache::thrift::transport;
using namespace ::apache::thrift::server;
using namespace ::save_service;
using namespace ::match_service;
using namespace std;
using namespace apache::thrift::concurrency;

struct Task{
    User user;
    string type;
};

struct MessageQueue{
    queue<Task> q;
    mutex m;
    condition_variable cv;
}message_queue;

class Pool{
    public:
        void save_result(User &a, User &b)
        {
            printf("match %d %d\n", a.id, b.id);
            std::shared_ptr<TTransport> socket(new TSocket("123.57.67.128", 9090));
            std::shared_ptr<TTransport> transport(new TBufferedTransport(socket));
            std::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
            SaveClient client(protocol);

            try {
                transport->open();


                int res = client.save_data("acs_11751", "0327ce99", a.id, b.id);
                if (!res) printf("success\n");
                else printf("failed\n");


                transport->close();
            } catch (TException& tx) {
                cout << "ERROR: " << tx.what() << endl;
            }
        }
        void match()
        {
            if (users.size() > 1)
            {
                sort(users.begin(), users.end(), [](User &a, User &b){
                        return a.score < b.score;
                        });

                for (uint32_t i = 1; i < users.size(); i ++ )
                {
                    auto a = users[i], b = users[i - 1];
                    if ( a.score - b.score < 50 )
                    {
                        users.erase(users.begin() + i);
                        users.erase(users.begin() + i - 1);
                        save_result(a, b);

                        break;
                    }
                }
            }
        }
        void add(User user)
        {
            users.push_back(user);
        }
        void remove(User user)
        {
            for (uint32_t i = 0; i < users.size(); i ++ )
            {
                if (users[i].id == user.id)
                {
                    users.erase(users.begin() + i);
                    break;

                }
            }
        }
    private:
        vector<User> users;

}pool;


class MatchHandler : virtual public MatchIf {
    public:
        MatchHandler() {
            // Your initialization goes here
        }

        int32_t add_user(const User& user, const std::string& info) {
            // Your implementation goes here
            unique_lock<mutex> lck(message_queue.m);
            printf("add_user\n");
            message_queue.q.push({user, "add"});
            message_queue.cv.notify_all();
            return 0;
        }

        int32_t remove_user(const User& user, const std::string& info) {
            // Your implementation goes here
            unique_lock<mutex> lck(message_queue.m);
            printf("remove_user\n");
            message_queue.q.push({user, "remove"});
            message_queue.cv.notify_all();
            return 0;
        }

};

class MatchCloneFactory : virtual public MatchIfFactory {
    public:
        ~MatchCloneFactory() override = default;
        MatchIf* getHandler(const ::apache::thrift::TConnectionInfo& connInfo) override
        {
            std::shared_ptr<TSocket> sock = std::dynamic_pointer_cast<TSocket>(connInfo.transport);
            cout << "Incoming connection\n";
            cout << "\tSocketInfo: "  << sock->getSocketInfo() << "\n";
            cout << "\tPeerHost: "    << sock->getPeerHost() << "\n";
            cout << "\tPeerAddress: " << sock->getPeerAddress() << "\n";
            cout << "\tPeerPort: "    << sock->getPeerPort() << "\n";
            return new MatchHandler;
        }
        void releaseHandler(MatchIf* handler) override {
            delete handler;
        }
};

void consume_task()
{
    while (true)
    {
        unique_lock<mutex> lck(message_queue.m);
        if (message_queue.q.empty())
        {
            lck.unlock();
            pool.match();
            sleep(1);
        }
        else
        {
            auto t = message_queue.q.front();
            message_queue.q.pop();
            lck.unlock();

            //判断任务类型，往匹配池中添加用户或删除用户。
            if (t.type == "add")
            {
                pool.add(t.user);
            }
            else if(t.type == "remove")
            {
                pool.remove(t.user);
            }
            pool.match();
        }
    }
}

int main(int argc, char **argv) {
    TThreadedServer server(
            std::make_shared<MatchProcessorFactory>(std::make_shared<MatchCloneFactory>()),
            std::make_shared<TServerSocket>(9090), //port
            std::make_shared<TBufferedTransportFactory>(),
            std::make_shared<TBinaryProtocolFactory>());
    cout << "Match Server running" <<endl;
    thread matching_thread(consume_task);
    server.serve();
    return 0;
}

