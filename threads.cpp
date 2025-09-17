#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <iostream>
#include <limits>
#include <mutex>
#include <thread>
#include <unistd.h>
#include <vector>

using namespace std;

namespace {
constexpr int kThreadCount = 3;
constexpr chrono::milliseconds kPauseCheck{50};
constexpr int kMinIntervalMs = 100;
}

atomic<bool> runFlag{true};
atomic<bool> menuPause{false};
atomic<int> messageIntervalMs{500};
mutex coutMutex;
array<atomic<bool>, kThreadCount> threadActive;

void printLocked(const string &text) {
    lock_guard<mutex> lock(coutMutex);
    cout << text << endl;
}

void threadFunc(int id) {
    auto &activeFlag = threadActive[id - 1];

    // потоки стартуют приостановленными и ждут команды от пользователя
    while (runFlag.load()) {
        if (menuPause.load()) {
            this_thread::sleep_for(kPauseCheck);
            continue;
        }

        if (!activeFlag.load()) {
            this_thread::sleep_for(chrono::milliseconds(100));
            continue;
        }

        {
            lock_guard<mutex> lock(coutMutex);
            cout << "Поток " << id << " выполняет работу." << endl;
        }

        const int interval = messageIntervalMs.load();
        int elapsed = 0;
        while (elapsed < interval && runFlag.load() && activeFlag.load() && !menuPause.load()) {
            const int step = min(100, interval - elapsed);
            this_thread::sleep_for(chrono::milliseconds(step));
            elapsed += step;
        }
    }

    lock_guard<mutex> lock(coutMutex);
    cout << "Поток " << id << " завершён." << endl;
}

void showStatus() {
    lock_guard<mutex> lock(coutMutex);
    cout << "\nСостояние потоков:" << endl;
    for (int i = 0; i < kThreadCount; ++i) {
        cout << "  Поток " << (i + 1) << ": "
             << (threadActive[i].load() ? "работает" : "приостановлен")
             << endl;
    }
}

int main() {
    for (auto &flag : threadActive) {
        flag.store(false);
    }

    vector<thread> threads;
    threads.reserve(kThreadCount);
    for (int i = 1; i <= kThreadCount; ++i) {
        threads.emplace_back(threadFunc, i);
    }

    const bool interactiveInput = ::isatty(STDIN_FILENO);
    auto waitForEnter = [&]() {
        if (!interactiveInput) {
            menuPause.store(false);
            this_thread::sleep_for(chrono::milliseconds(messageIntervalMs.load()));
            menuPause.store(true);
            return;
        }

        if (cin.rdbuf()->in_avail() > 0) {
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
        }

        {
            lock_guard<mutex> lock(coutMutex);
            cout << "Нажмите Enter для возврата в меню..." << endl;
            cout.flush();
        }

        menuPause.store(false);
        cin.get();
        menuPause.store(true);
    };

    int choice = -1;
    while (runFlag.load()) {
        menuPause.store(true);
        {
            lock_guard<mutex> lock(coutMutex);
            cout << "\nМеню управления:" << endl
                 << "1 - Показать состояние потоков" << endl
                 << "2 - Запустить/приостановить поток" << endl
                 << "3 - Установить интервал вывода (мс)" << endl
                 << "4 - Запустить все потоки" << endl
                 << "5 - Приостановить все потоки" << endl
                 << "0 - Выход" << endl
                 << "> ";
            cout.flush();
        }

        if (!(cin >> choice)) {
            cin.clear();
            cin.ignore(numeric_limits<streamsize>::max(), '\n');
            printLocked("Некорректный ввод. Попробуйте снова.");
            menuPause.store(false);
            continue;
        }

        bool waitAfterAction = true;

        switch (choice) {
        case 1:
            showStatus();
            break;
        case 2: {
            {
                lock_guard<mutex> lock(coutMutex);
                cout << "Введите номер потока (1-" << kThreadCount << "): ";
                cout.flush();
            }

            int id = 0;
            if (!(cin >> id)) {
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                printLocked("Некорректный ввод номера потока.");
                break;
            }

            if (id < 1 || id > kThreadCount) {
                printLocked("Такого потока не существует.");
                break;
            }

            const bool newState = !threadActive[id - 1].load();
            threadActive[id - 1].store(newState);
            {
                lock_guard<mutex> lock(coutMutex);
                cout << "Поток " << id
                     << (newState ? " запущен." : " приостановлен.")
                     << endl;
            }

            break;
        }
        case 3: {
            {
                lock_guard<mutex> lock(coutMutex);
                cout << "Введите интервал в миллисекундах (не менее "
                     << kMinIntervalMs << "): ";
                cout.flush();
            }

            int interval = 0;
            if (!(cin >> interval)) {
                cin.clear();
                cin.ignore(numeric_limits<streamsize>::max(), '\n');
                printLocked("Некорректный ввод интервала.");
                break;
            }

            interval = max(interval, kMinIntervalMs);
            messageIntervalMs.store(interval);
            {
                lock_guard<mutex> lock(coutMutex);
                cout << "Новый интервал сообщений: " << interval << " мс." << endl;
            }
            break;
        }
        case 4:
            for (auto &flag : threadActive) {
                flag.store(true);
            }
            printLocked("Все потоки запущены.");
            break;
        case 5:
            for (auto &flag : threadActive) {
                flag.store(false);
            }
            printLocked("Все потоки приостановлены.");
            break;
        case 0:
            runFlag.store(false);
            waitAfterAction = false;
            break;
        default:
            printLocked("Нет такого пункта меню.");
            break;
        }

        if (!runFlag.load()) {
            menuPause.store(false);
            break;
        }

        if (waitAfterAction) {
            waitForEnter();
        } else {
            menuPause.store(false);
            this_thread::sleep_for(kPauseCheck);
            menuPause.store(true);
        }
    }

    for (auto &flag : threadActive) {
        flag.store(false);
    }

    for (auto &t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }

    printLocked("Программа завершена.");
    return 0;
}
