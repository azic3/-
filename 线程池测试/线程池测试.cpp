#include <iostream>
#include "threadpool.h"
#include <chrono>
#include <thread>
#include <any>
class MyTask : public Task {
public:
    MyTask(int begin, int end) : begin_(begin), end_(end) {}

    // 返回值自动隐式转换为 Any (或 std::any)
    Any run() override {

        // 模拟耗时操作，注意在实际测试中 10 秒可能较长，可酌情缩短

         std::this_thread::sleep_for(std::chrono::seconds(1)); 

        int sum = 0;
        for (int i = begin_; i <= end_; i++) {
            sum += i;
        }

        return sum; // 隐式构造 Any(int)
    }

private:
    int begin_;
    int end_;
};

int main() 
{
    {
        ThreadPool pool;
        pool.setMode(ModeThread::MODE_CACHED);
        pool.start(2);
        std::shared_ptr<Result> sp_res1 = pool.submitTask(std::make_shared<MyTask>(1, 10000));
        std::shared_ptr<Result> sp_res2 = pool.submitTask(std::make_shared<MyTask>(1, 10000));
        std::shared_ptr<Result> sp_res3 = pool.submitTask(std::make_shared<MyTask>(1, 10000));
        std::shared_ptr<Result> sp_res4 = pool.submitTask(std::make_shared<MyTask>(1, 10000));
        std::shared_ptr<Result> sp_res5= pool.submitTask(std::make_shared<MyTask>(1, 10000));
        std::shared_ptr<Result> sp_res6= pool.submitTask(std::make_shared<MyTask>(1, 10000));
        //Result& res1 = *sp_res1;

        //int sum1 = res1.get().cast_<int>();
        //std::cout << sum1 << std::endl;
    }
   
    std::cout << " main end ()! " << std::endl;
    getchar();
#if 0
    ThreadPool pool; 
    pool.setMode(ModeThread::MODE_CACHED);
    pool.start(2);
   
    std::cout << "\n========== 阶段一：提交大批量任务，触发自动扩容 ==========" << std::endl;
    std::vector<std::shared_ptr<Result>> results;

    // 瞬间提交 10 个耗时任务 (每个耗时3秒)
    // 初始2个线程会被瞬间占满，剩下的8个任务会导致 taskSize > idleThreadSize
    // 从而清晰地触发 submitTask 里的 "Thread  Creat  susseed ! " 打印
    for (int i = 0; i < 10; ++i) {
        int begin = i * 10000 + 1;
        int end = (i + 1) * 10000;
        results.push_back(pool.submitTask(std::make_shared<MyTask>(begin, end)));
    }

    std::cout << "\n========== 阶段二：等待所有任务执行完毕 ==========" << std::endl;
    int totalSum = 0;
    // 依次阻塞获取结果
    for (int i = 0; i < 10; ++i) {
        totalSum += results[i]->get().cast_<int>();
    }
    std::cout << "✅ 所有任务执行完毕，总和 Total Sum: " << totalSum << std::endl;

    std::cout << "\n========== 阶段三：主线程休眠12秒，触发多余线程自杀机制 ==========" << std::endl;
    // 因为 THREAD_MAS_IDLE_TIME 是 10 秒
    // 等待 12 秒可以让空闲的多余线程触发 timeout 逻辑并 exit()
    std::this_thread::sleep_for(std::chrono::seconds(10));

    std::cout << "\n========== 测试结束 ==========" << std::endl;

    getchar();
#endif
    return 0;
}
   