///
/// @file    queryTask.cc
/// @author  
///

#include "queryTask.h"
#include "configuration.h"
#include "wordQuery.h"
#include "cppLog.h"
#include <string.h>
namespace cc
{

	QueryTask::QueryTask(cc::TcpConnectionPtr spConn,WordQuery * wordQueryPtr,RedisPool *redisPtr,string & queryStr)
		:_spConn(spConn)
		 ,_wordQueryPtr(wordQueryPtr)
		 ,_redisPtr(redisPtr)
		 ,_queryStr(queryStr)
	{}


	void QueryTask::process(){//处理查词任务（由子线程执行）
		string hashKey = Configuration::getInstance().getConfMap()["CacheHashKey"];//hashkey的名称
		string field = _queryStr;//键值对的key
		string result;
		string logMessage = _spConn->toString() + " [--send result--] "+_queryStr;//日志信息
		bool isFound = _redisPtr->ExecuteCmdBy_3_InputArgs("HGET",hashKey,field,result);
		if(!isFound || result.empty()){//在缓存中没找到,执行查询
			auto ret = _wordQueryPtr->doQuery(_queryStr);
			_spConn->sendAndClose(ret);
			//写入缓存
			_redisPtr->ExecuteCmdBy_4_InputArgs("HSET",hashKey,field,ret,result);
		}else{//发送缓存中的数据
			_spConn->sendAndClose(result);
		}
		//日志
		logInfo(logMessage.c_str());
	}

};//namespace cc
