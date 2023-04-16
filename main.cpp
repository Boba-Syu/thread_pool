#include <iostream>
#include "threadTest.h"


using namespace std;

int fun1(int id) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    cout << "fun1: " << id << endl;
    return id;
}

struct fun2 {
    void operator()(int id) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        cout << "fun2: " << id << endl;
    }
};

int main() {
    ThreadPool threadPool(3);

    std::future<int> ret = threadPool.push(fun1);
    threadPool.push(fun2());
    cout << ret.get() << endl;

    auto fun3 = [](int id) {
        cout << "fun3: " << id << endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        return "fun3";
    };
    threadPool.push(fun3);

    auto fun4 = [](int id, string &&f) {
        cout << "fun4: " << id << ' ' << f << endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    };
    threadPool.push(fun4, "fun");

    struct fun5 {
        string operator()(int id, string a1, string a2) {
            cout << "fun5: " << id << endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            return a1 + a2;
        }
    };
    std::future<string> res = threadPool.push(fun5(), "a1", "a2");
    cout << res.get() << endl;

    return 0;
}

