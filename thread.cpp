#include <iostream>      // std::cout, std::endl
#include <thread>        // std::thread, this_thread::get_id()
#include <chrono>        // 時間相關，chrono::system_clock
#include <iomanip>       // std::setw, std::setfill, std::put_time
#include <atomic>        // std::atomic，用於原子操作保證多執行緒安全
#include <vector>        // std::vector，用於管理多個 thread
#include <string>        // std::string
#include <sstream>       // std::stringstream，用於格式化時間字串

using namespace std;

/*
  ANSI 顏色控制：
      - \033[31m : 紅色
      - \033[32m : 綠色
      - \033[33m : 黃色
      - \033[34m : 藍色
      - \033[35m : 紫色
      - \033[0m  : 重置顏色
  用途：
      - 區分不同執行緒輸出
      - 提高多執行緒輸出可讀性
*/

/*
  全域原子變數 seq_counter:
      - 用於給每個輸出分配唯一序號
      - std::atomic 保證 fetch_add 在多執行緒同時操作下不會 race condition
      - fetch_add(1) 回傳目前值後自動加 1
      - 使用序號可追蹤各執行緒輸出順序
*/
atomic<int> seq_counter(1);

/*
  current_timestamp():
      - 功能：取得當前本地時間字串，精準到毫秒
      - chrono::system_clock::now() 取得系統時間點
      - duration_cast<milliseconds> 將時間點轉為毫秒數
      - localtime_s / localtime_r 將 time_t 轉成本地時間結構 tm
      - stringstream 與 put_time 用於格式化輸出
      - 輸出格式 [HH:MM:SS.mmm]
*/
string current_timestamp() {
    using namespace chrono;
    auto now = system_clock::now();
    auto ms = chrono::duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
    auto t_c = chrono::system_clock::to_time_t(now);
    tm local_tm;
#ifdef _WIN32
    localtime_s(&local_tm, &t_c);  // Windows 安全函式
#else
    localtime_r(&t_c, &local_tm);  // Linux/Unix 安全函式
#endif
    stringstream ss;
    ss << put_time(&local_tm, "%H:%M:%S") << "."
        << setfill('0') << setw(3) << ms.count();
    return ss.str();
}

/*
  print_thread_output():
      - 統一多執行緒輸出格式
      - 輸出包含：
          * ANSI 顏色
          * 時間戳記
          * 全局序號
          * 標籤 (thread 名稱)
          * 函式名稱 (__func__)
          * thread ID
      - seq_counter 使用 atomic 保證多執行緒同時輸出時序號唯一
      - ANSI 顏色用完後需重置 (\033[0m)
      - 可在教學或實務專案中快速識別 thread
*/
void print_thread_output(const string& color, const string& label, const string& func_name) {
    int seq = seq_counter.fetch_add(1);  // 取得唯一序號

    // 用 stringstream 組整段輸出
    stringstream ss;
    ss << color
        << "[" << current_timestamp() << "] "
        << "#" << seq << " [" << label << "]\n"
        << "  Function : " << func_name << "\n"
        << "  Thread ID: " << this_thread::get_id() << "\n"
        << "---------------------------------\n";

    // 一次性輸出，避免多執行緒混亂
    cout << ss.str() << "\033[0m";
}


/*
  free_function():
      - 範例自由函式
      - 用於展示 std::thread 可執行自由函式
      - 顏色綠色 (\033[32m)
      - 輸出由 print_thread_output 管理
      - 適合展示獨立執行緒行為
*/
void free_function(int id) {
    print_thread_output("\033[32m", "free_function #" + to_string(id), __func__);
}

/*
  class Foo:
      - 範例類別
      - 成員函式 member_function 用於展示 std::thread 可執行物件成員函式
      - 每個物件可被多個 thread 呼叫
*/
class Foo {
public:
    /*
      member_function():
          - 藍色 (\033[34m)
          - 由 thread 執行
          - 輸出格式與自由函式統一
          - id 用於標示不同物件的 thread
    */
    void member_function(int id) {
        print_thread_output("\033[34m", "Foo::member_function #" + to_string(id), __func__);
    }
};

int main() {
    /*
      主程式 main():
          - 主執行緒
          - 顏色黃色 (\033[33m)
          - 顯示序號、函式名稱、Thread ID
          - 教學上可觀察主程式與子執行緒差異
    */
    print_thread_output("\033[33m", "main", __func__);

    /*
      查詢 CPU 硬體執行緒數量：
          - thread::hardware_concurrency()
          - 回傳 unsigned int
          - 表示 CPU 邏輯核心數
      const auto size:
          - const → size 為常數，初始化後不可修改
          - auto  → 編譯器自動推斷型別 (unsigned int)
    */
    const auto size = thread::hardware_concurrency();
    cout << "[" << current_timestamp() << "] "
        << "Hardware concurrency: " << size << "\n"
        << "---------------------------------\n";

    // 建立多個 Foo 物件
    const int foo_count = 4; // 建立 4 個 Foo 物件
    vector<Foo> foos(foo_count);

    // 容器存放所有 thread
    vector<thread> threads;

    /*
      建立 Foo 成員函式 thread:
          - 每個 thread 呼叫不同物件
          - 傳入物件指標 &foos[i] 保證呼叫原始物件而非複製
          - id 參數標示不同物件的 thread
    */
    for (int i = 0; i < foo_count; ++i) {
        threads.emplace_back(&Foo::member_function, &foos[i], i + 1);
    }

    /*
      建立自由函式 thread:
          - 3 個自由函式 thread
          - 每個 thread 可並行執行
          - id 參數標示不同自由函式 thread
    */
    const int free_count = 3;
    for (int i = 0; i < free_count; ++i) {
        threads.emplace_back(free_function, i + 1);
    }

    /*
      等待所有 thread 完成:
          - joinable() 檢查 thread 是否可 join
          - join() 等待 thread 結束
          - 確保主程式結束前所有 thread 已完成
          - 避免未 join thread 導致程式異常結束
    */
    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }

    /*
      程式結束：
          - 所有子執行緒已完成
          - 主執行緒 main() 結束
          - seq_counter 保證每個輸出都有唯一序號
    */
    return 0;
}