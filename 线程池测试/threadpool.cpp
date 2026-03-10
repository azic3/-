#include "threadpool.h"
#include <thread>
#include <iostream>
#include <mutex>
#include <chrono>
const int TASK_MAX_THRESHHOLD = 1024;
const int Thread_MAX_THRESHHOLD = 200;
const int THREAD_MAS_IDLE_TIME = 30;  //单位秒
//线程池构造
ThreadPool::ThreadPool() :initThreadSize_(0)
, taskSize_(0)
, taskQueMaxThreadHold_(TASK_MAX_THRESHHOLD)
, threadSizeThreadHold(Thread_MAX_THRESHHOLD)
, idleThreadSize(0)
,PoolMode(ModeThread::MODE_FIXED)
, isPoolThreadRunning_(false)
{}

//线程池析构,线程池资源回收！
ThreadPool::~ThreadPool()
{
	
	isPoolThreadRunning_ = false;//运行状态为false。
    
	//等待线程池中的线程返回。//两种状态 1是--阻塞 2是--正在执行任务。
	std::unique_lock<std::mutex>lock(taskQueMtx_);
	notEmpty_.notify_all();
	exitCond_.wait(lock, [this]()->bool {return threads_.size() == 0; });
	
}
//设定线程池模式
void ThreadPool::setMode(ModeThread mode)
{
	if (checkPooolThreadRunning())
		return;
	//MODE_FIXED, //固定模式
	//MODE_CACHED //动态增长模式
	PoolMode = mode;

}

//设置task任务队列上线阈值
void ThreadPool::setTaskQueMaxThreshHold(int threshhold)
{
	if (checkPooolThreadRunning())
		return;
	taskQueMaxThreadHold_ = threshhold;

}

//在cached模式下设置线程池中线程的上限阈值。
void ThreadPool::setThreadHold(int threshhold)
{
	if (checkPooolThreadRunning())
		return;
	if (PoolMode == ModeThread::MODE_CACHED)
	{
		threadSizeThreadHold = threshhold;
	}

}

//给线程池提交任务---------//非常重要
std::shared_ptr<Result> ThreadPool::submitTask(std::shared_ptr<Task> sp)
{
	// 获取锁
	std::unique_lock<std::mutex> lock(taskQueMtx_);

	//在cached模式下，有可能创建多了线程，但是空闲数量超过60后因该把多余的线程给结束了。
	if (PoolMode == ModeThread::MODE_CACHED
		&& taskSize_ > idleThreadSize
		&& curTotalThread < threadSizeThreadHold)
	{
		//创建新的线程 老版的绑定器----------bind()
		/*auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc,this, std::placeholders::_1));
		int ThreadId = ptr->getId();
		ptr->start();
		threads_.emplace(ThreadId, std::move(ptr));*/

		//新版用Lambda创建新的线程，绑定线程函数。
		auto ptr = std::make_unique<Thread>([this](int id) {
			this->threadFunc(id);
			});
		int ThreadId = ptr->getId();

		// 2. 将独占智能指针的所有权安全地转移给 map 容器
		threads_.emplace(ThreadId, std::move(ptr));

		// 3. 必须先启动底层线程，让它跑起来
		threads_[ThreadId]->start();
		std::cout << "Thread  Creat  susseed ! " << std::endl;
		//空闲线程的数量要++
		idleThreadSize++;

		//线程池中线程总量也要++
		curTotalThread++;
	}



	// 1. 超时判断逻辑（原样保留，非常棒）
	if (!notFull_.wait_for(lock, std::chrono::seconds(1), [this]()->bool {
		return taskQue_.size() < (size_t)taskQueMaxThreadHold_;
		}))
	{
		std::cerr << "submit task timeout!" << std::endl;
		// 失败：返回一个 isValid = false 的 Resulat 指针
		return std::make_shared<Result>(false);
	}
	// 1. 先创建 Result
	auto res = std::make_shared<Result>(true);
	// 2. 必须在入队前，把 Result 牢牢绑定到 Task 上！
	sp->setResult(res);

	// 3. 队列未满，加入任务
	taskQue_.emplace(sp);
	taskSize_++;

	// 4. 唤醒一个处于阻塞状态的消费者（工作线程）
	// 优化：使用 notify_one() 而不是 notify_all()，避免惊群效应
	notEmpty_.notify_one();

	//需要根据任务的数量和空闲线程的数量，判断是否需要新的线程创建出来。

	// 5. 将获取结果的“凭证”返回给用户
	return res;
}
//Result ThreadPool::submitTask(std::shared_ptr<Task> sp)
//{
//	//获取锁
//	std::unique_lock<std::mutex> lock(taskQueMtx_);
//	
//	//线程通信 等待任务队列有空余
//	
//	// "我等待，直到队列不满"
//	// 这里的 lambda 意思是：如果队列不满，返回 true，wait 结束。
//	// 如果队列满了，返回 false，wait 释放锁并阻塞。
//	//用户提交任务，最长不能阻塞超过1s,否则判断提交任务失败，返回
//
//	if (!notFull_.wait_for(lock, std::chrono::seconds(1), [this]()->bool {
//		return taskQue_.size() <(size_t)taskQueMaxThreadHold_;
//		}))
//	{
//		std::cerr << "submit task timeout!" << std::endl;
//		return Result(sp,false);
//	}
//	
//	//传统写法
//	/*while (taskQue_.size() == taskQueMaxThreadHold_)
//	{
//		notFull_.wait(lock);
//	}*/
//
//	// 队列未满，加入任务
//
//	taskQue_.emplace(sp);
//	taskSize_++;
//	// 通知消费者线程
//	notEmpty_.notify_all();
//
//}

//开启线程池
void ThreadPool::start(int initThradSize)
{
	//设置线程池的状态
	
	isPoolThreadRunning_ = true;

	//记录初始线程个数
	initThreadSize_ = initThradSize;
	curTotalThread = initThradSize;

	//创建线程对象
	for (int i = 0; i < initThreadSize_;i++)
	{
		//创建thread线程对象的时候，把线程函数（threadFunc()）给到thread线程对象方法一用传统的bind进行绑定，方法二用Lambda来表达式捕获 this 指针
		//threads_.emplace_back(new Thread(std::bind(&ThreadPool::threadFunc, this)));

		/*auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		int ThreadId = ptr->getId();
		ptr->start();
		threads_.emplace(ThreadId, std::move(ptr));*/


		auto ptr = std::make_unique<Thread>([this](int id) {
			this->threadFunc(id);
			});
		int ThreadId = ptr->getId();

		// 【关键修复】：先启动，再 move 进 map
		ptr->start();
		threads_.emplace(ThreadId, std::move(ptr));
		idleThreadSize++;
		

	}
	

}
//定义线程函数
void ThreadPool::threadFunc(int threadid)
{
	//C++11 初始化开始时间
	auto lastime = std::chrono::high_resolution_clock().now();
	//所有任务必须执行完
	for(;;)
	{
		    std::shared_ptr<Task> task;
		//出了这个作用域，lock这把锁会自动释放，其他的线程就可以拿到锁。不会让这个线程一直拿这把锁在手上。
		{   
			
			//先获取锁。
			std::unique_lock<std::mutex> lock(taskQueMtx_);

			std::cout << "tid:" << std::this_thread::get_id()
			<< "正在获取任务" << std::endl;
				//cached模式下，有可能已经创建了很多的线程，但是空闲时间超过60s，
				//应该把多余的线程给删去
			
				//每一秒中返回一次 怎么区分超时返回，还是有任务执行返回
			 //锁+ 双重判断！
				while (taskQue_.size() == 0)
				{
					//线程池资源要回收！
					if (!isPoolThreadRunning_) {
						threads_.erase(threadid);
						std::cout << "threadid :" << std::this_thread::get_id() << ""
							<< "eixt()!" << std::endl;
						exitCond_.notify_all();
						return;
					}
					if (PoolMode == ModeThread::MODE_CACHED)
					{
						//条件变量超市返回，计算当前的时间
						if (std::cv_status::timeout == notEmpty_.wait_for(lock, std::chrono::seconds(1)))
						{
							auto now = std::chrono::high_resolution_clock().now();
							auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastime);
							if (dur.count() >= THREAD_MAS_IDLE_TIME
								&& curTotalThread > initThreadSize_)
							{
								//开始回收线程。
								//记录线程数量相关进行修改
								//把线程对象从线程列表容器中删除。

								//找线程id---Threadid,找线程id进行删除。
								threads_.erase(threadid);
								curTotalThread--;
								idleThreadSize--;
								std::cout << "threadid :" << std::this_thread::get_id()
									<< "eixt()!" << std::endl;
								return;
							}
						}
					}
					else
					{
						//等待notEmpty条件。
						notEmpty_.wait(lock);
					
					}
					////线程池资源回收！
					//if (!isPoolThreadRunning_)
					//{
					//	threads_.erase(threadid);
					//	std::cout << "threadid :" << std::this_thread::get_id()
					//		<< "eixt()!" << std::endl;
					//	exitCond_.notify_all();
					//	return;
					//}
				}
				
			
			idleThreadSize--;
			std::cout << "tid:" << std::this_thread::get_id()
			<< "获取任务成功" << std::endl;
			//从任务队列中取一个任务出来。
			task = taskQue_.front();
			taskQue_.pop();
			taskSize_--;
			//取出一个任务，任务队列中的任务还是大于0的化,告诉其他线程来取出任务来执行(等于消费者来消费任务)
			if (taskQue_.size() > 0)
			{
				notEmpty_.notify_all();
			}

			//取出一个任务通知生产者，仓库不满了
			notFull_.notify_all();
		}
		//--------------------------------------------------------------
		//当前线程负责执行这个任务。
		if (task != nullptr)
		{
			task->exec();//任务执行完
		}
		
		idleThreadSize++;
		lastime= std::chrono::high_resolution_clock().now();
	}


}
bool ThreadPool::checkPooolThreadRunning()
{
	return isPoolThreadRunning_;
}

///////////线程方法实现
//线程构造
Thread::Thread(threadFunc func) :func_(func)
, Threadid_(generateId++)
{

}
//线程析构
Thread::~Thread()
{


}
int Thread::generateId = 0;
//启动线程
void Thread::start()
{
	//创建一个线程来执行一个线程函数
	std::thread t(func_,Threadid_);
	t.detach();

}
int  Thread::getId()const
{

	return Threadid_;
}
