#include <uWebSockets/App.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

using namespace std;
using namespace uWS;
using namespace nlohmann;

// store something you care about for a client
struct ConnectionData{
    vector<string> topic;
    string_view address;
    static int cnt;
    ConnectionData() = default;
};
int ConnectionData::cnt = 0;

vector<string> globalTopic;

mutex mtx;
condition_variable cv;
bool clientSubscribe = false;

int main() {
    string ip = "127.0.0.1";
    int port = 1256;
    SSLApp sslApp = SSLApp (SocketContextOptions {
        // Where the certificates (Produced by OpenSSL) are placed
            .key_file_name = "../misc/key.pem",
            .cert_file_name = "../misc/cert.pem",
            .passphrase = "1234"
    }).ws<ConnectionData>("/*", SSLApp::WebSocketBehavior<ConnectionData>{
        // 5 minutes to timeout
        .idleTimeout = 300,
        .upgrade = [](HttpResponse<true>* res, HttpRequest* req, auto* context) {

            res->template upgrade<ConnectionData>(ConnectionData{
                // store client address in ConnectionData (one ConnectionData per client)
              .address = res->getRemoteAddressAsText()
            },
            req->getHeader("sec-websocket-key"),
            req->getHeader("sec-websocket-protocol"),
            req->getHeader("sec-websocket-extensions"),
            context);
        },
        .message = [&](WebSocket<true, true, ConnectionData>* ws, std::string_view message, OpCode opCode) {
            // parse the message sent by client into JSON format and loop up what topic it wants to subscribe
            json messageInJSON = json::parse(message);
            string subscribeTopic = messageInJSON["topic"].dump();

            // record the topic in client`s ConnectionData and globalTopic
            ws->getUserData()->topic.emplace_back(subscribeTopic);
            globalTopic.emplace_back(subscribeTopic);

            // subscribe it
            ws->subscribe(subscribeTopic);

            // tell the thread who runs publishTopicRandomly that can publish topic
            {
                unique_lock<mutex> lock;
                clientSubscribe = true;
                cv.notify_all();
            }
        },
    }).listen(ip, port, [&](auto *listen_socket) {
        if (listen_socket) {
            std::cout << "Listening on port " << port << std::endl;
        }
    });

    auto publishTopicRandomly = [&]() {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [](){return clientSubscribe;});
        int times = 10;
        while (times--) {
            int index = rand() % globalTopic.size();
            sslApp.publish(globalTopic[index], globalTopic[index] + " happens", OpCode::TEXT);
            cout << globalTopic[index] + " published" << endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        }
    };

    thread t(publishTopicRandomly);
    sslApp.run();

    t.join();

    return 0;
}