#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <vector>
#include <queue>
#include <memory>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <semaphore>
//Any类型: 可以接收任意类型
class Any
{
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete; 
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;


	//这个构造函数可以让Any类型接收任意的类型
	template<typename T>
	Any(T data):base_(std::make_unique<Derive<T>>(data)){}

	//这个方法可以把Any对象里面存储的data取出来
	template<typename T>
	T cast_()
	{
		//我们怎么从base_找到它所指向的Derive对象，从它里面取到data成员变量。
		//基类指针=》 派生类指针 RTTI
		Derive<T>* pd = dynamic_cast<Derive<T>*>(base_.get());
		if (pd == nullptr)
		{
			throw " type is unmatch";
		}
		return pd->data_;
	}


private:               
	//基类类型
	class Base
	{
	public:
		virtual ~Base() = default;
	};
	//派生类类型
	template <typename T>
	class Derive :public Base
	{
	public:
		Derive(T data) :data_(data){}
		T data_;//保存其他类型的
	};
private:
	std::unique_ptr<Base> base_;
};




//信号量类SemaPhone
class SemaPhone
{
public:
	SemaPhone(int limit = 0)
		:resLimit_(limit) 
	{}
	~SemaPhone()
	{}
	//获取一个信号量资源
	void wait()
	{
		std::unique_lock<std::mutex> lock(mutex_);
		//等待信号量有资源，如果有资源的话，没资源的话就阻塞当前线程。

		cond_.wait(lock, [&]()->bool {return resLimit_ > 0; });

		resLimit_--;

	}
	//增加一个信号量资源
	void post()
	{
		std::unique_lock<std::mutex> lock(mutex_);
		resLimit_++;
		cond_.notify_all();
	}

	SemaPhone(const SemaPhone&) = delete;
	SemaPhone& operator=(const SemaPhone&) = delete;

private:
	int resLimit_;
	std::mutex mutex_;
	std::condition_variable cond_;
};




//Result类型，接收任务执行完的返回值。
class Task;//类的声明
class Result {
public:
	Result(bool isValid = true) : isValid_(isValid) {}
	~Result() = default;

	void setVal(Any any) {
		any_ = std::move(any);
		sem_.release(); // 相当于原代码的 post()，释放信号量
	}

	Any get() {
		if (!isValid_) {
			return {}; // 返回空 Any
		}
		sem_.acquire(); // 相当于原代码的 wait()，阻塞等待结果
		return std::move(any_);
	}

private:
	Any any_;
	std::binary_semaphore sem_{ 0 }; // 初始为0的二元信号量
	std::atomic_bool isValid_;
};
/*class Result
{
	
public:
     Result(std::shared_ptr<Task>task, bool isValid = true);
	~Result() = default;
	//问题一：setVal方法，获取任务执行完的返回值的
	void setVal(Any any);
	

	//问题二：get方法，用户调用这个方法获取task的返回值。
	Any get();
	

private:
	Any Any_;//存储任务的返回值。
	SemaPhone sem_;//线程通信。
	//std::shared_ptr<Task> task_;//指向对应获取返回值的任务对象。
	std::atomic_bool isValid_;//返回值是否有效


};
*/
//任务抽象基类
// 2. Task：负责执行逻辑并写入结果
class Task {
public:
	Task() = default;
	virtual ~Task() = default; // 虚析构函数，保证派生类正确析构

	// 线程池工作线程会调用此函数
	void exec() {
		if (result_) {
			// 执行多态 run() 并将返回值塞入 Result
			result_->setVal(run());
		}
	}

	void setResult(std::shared_ptr<Result> res) {
		result_ = res;
	}

	virtual Any run() = 0;

private:
	std::shared_ptr<Result> result_;
};


//线程池模式类型
enum class ModeThread
{
	MODE_FIXED, //固定模式
	MODE_CACHED //动态增长模式

};

//线程类型
class Thread
{
public:
	//线程函数对象类型
	using threadFunc = std::function<void(int)>;
	//线程构造
	Thread(threadFunc func);

	//线程析构
	~Thread();

	//启程线程
	void start();
	//获取线程id
	int getId()const;
private:
	threadFunc func_;
	static int generateId;
	int Threadid_;
};

//线程池类型
class ThreadPool
{
public:
	
	//线程池构造
	ThreadPool();

	//线程池析构
	~ThreadPool();

	//设定线程池模式
	void setMode(ModeThread mode);
	//在cached模式下设置线程池中线程的上限阈值。
	void setThreadHold(int threshhold);

	//设置task任务队列上线阈值
	void setTaskQueMaxThreshHold(int threshhold);

	//给线程池提交任务
	std::shared_ptr<Result> submitTask(std::shared_ptr<Task> sp);

	//开启线程池
	void start(int size=std::thread::hardware_concurrency());

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;
private:
	//定义线程函数
	void threadFunc(int threadid);
	bool checkPooolThreadRunning();

private:
	//std::vector<std::unique_ptr<Thread>> threads_; 
	std::unordered_map<int, std::unique_ptr<Thread>>threads_; //线程列表
	int initThreadSize_; //初始的线程数量
	int threadSizeThreadHold;//线程数量上限阈值。
	std::atomic_int idleThreadSize;           //空闲线程的数量
    std::atomic_int curTotalThread;//记录线程池中总线程数量

	std::queue<std::shared_ptr<Task>> taskQue_;//任务队列
	std::atomic_int taskSize_;//任务数量
	int taskQueMaxThreadHold_;//任务队列数量上限的阈值
	

	std::mutex taskQueMtx_;  //保证任务队列的线程安全
	std::condition_variable notFull_;//条件变量---不满
	std::condition_variable notEmpty_;//条件变量---不空
	std::condition_variable exitCond_; 


	ModeThread PoolMode;//线程池模式
	std::atomic_bool isPoolThreadRunning_;//线程池的状态
};

#endif 

