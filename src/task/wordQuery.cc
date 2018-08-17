///
/// @file    wordQuery.cc
/// @author  
///

#include "wordQuery.h"
#include "cppLog.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <json/json.h>
#include <unistd.h>
using std::cout;
using std::endl;

namespace cc
{

	WordQuery::WordQuery(Configuration & conf)
		:_conf(conf)
	{
		loadLibrary();
	}

	void WordQuery::loadLibrary(){//加载网页库到内存
		auto confMap = _conf.getConfMap();
		auto it = confMap.find("PageLibDir");
		if(it == confMap.end()){
			logErrorLoc("find confMap error");
			return;
		}
		string pageLibDir = it->second;
		std::ifstream ifsPageLib(pageLibDir + "pageLib.dat");
		std::ifstream ifsOffsetLib(pageLibDir + "offsetLib.dat");
		std::ifstream ifsInvertIndex(pageLibDir + "invertIndex.dat");
		if(!ifsPageLib.good() || !ifsOffsetLib.good() || !ifsInvertIndex.good()){
			logErrorLoc("open file error");
			return;
		}

		//读取网页库
		string line;
		int docId;
		long startPos,len;
		while(std::getline(ifsOffsetLib,line)){
			//将line转化成istringstream来读取配置信息
			std::istringstream iss(line);
			iss >> docId >> startPos >> len;
			//从ifs读取给定长度的字节
			ifsPageLib.seekg(startPos);//定位到指定的开头
			string doc(len,'0');//构造len长的string
			ifsPageLib.read(&doc[0],len);//读len长字符到string种
			WebPage wp(doc);
			wp.processDoc(_conf,WordSegmentation::getInstance());
			_pageLib.insert(make_pair(docId,wp));
		}


		//读取倒排索引信息
		string word;
		double weight;
		while(std::getline(ifsInvertIndex,line)){
			std::istringstream iss(line);
			iss >> word >> docId >> weight;//第一组倒排索引信息
			_invertIndexTable[word] = set<pair<int,double> >{{docId,weight}};
			while(iss >> docId >> weight){//剩余的倒排索引信息
				_invertIndexTable[word].insert(make_pair(docId,weight));
			}
		}
	}

#if 0
	// _invertIndexTable倒排索引格式unordered_map<word,set<pair<docId,weight> > > 
	vector<string,set<int> > docHasWords;
	vector<int,set<string> > iswordsOfDoc;//和查询词有交集文档，格式：vector<docId,set<word> >
	for(auto & word:queryWords){
		if(_invertIndexTable.count(word)){

		}
	}
#endif
	vector<double> WordQuery::getQueryWordsWeightVector(const vector<string> & queryWords){//计算查询词的权重值
		// _invertIndexTable倒排索引格式unordered_map<word,set<pair<docId,weight> > > 
		vector<double> queryWeights;
		int N = _pageLib.size();//网页文档总数
		for(const auto & word:queryWords){
			int df = 1;//文档频率
			if(_invertIndexTable.count(word)){
				df += _invertIndexTable[word].size();
			}
			double w = 1.0 * std::log(N*1.0 / df);//tf-idf
			queryWeights.push_back(w);
		}
		//归一化
		double sum = 0;
		for(auto & val:queryWeights){
			sum += val * val;
		}
		sum = std::sqrt(sum);
		for(auto & w:queryWeights){
			w = w*1.0 / sum;
		}
		return queryWeights;
	}

	bool compareByWeight(const pair<int,double> & v1,const pair<int,double> & v2){//按相似度排序
		return v1.second > v2.second;
	}
	vector<int> WordQuery::executeQuery(const vector<string> & queryWords,vector<pair<int,vector<double> > > & resultVec){//执行查询
		auto queryWeights = getQueryWordsWeightVector(queryWords);//计算查询次的权重
		vector<pair<int,double> > docIdByWeight;//文档Id，和与查询词的余弦相似度
		//计算余弦相似度
		for(auto & elem:resultVec){
			int docId = elem.first;
			vector<double> resultWeights = elem.second;
			double dotQR = 0.0;//x*y
			double uniQ = 0.0;//|x|
			double uniR = 0.0;//|y|
			for(size_t i = 0;i < queryWeights.size() && i < resultWeights.size();++i){
				double wq = queryWeights[i];
				double wr = resultWeights[i];
				dotQR += wq * wr;
				uniQ += wq * wq;
				uniR += wr * wr;
			}
			uniQ = std::sqrt(uniQ);
			uniR = std::sqrt(uniR);
			double cosSamilarity = dotQR / (uniQ * uniR);//余弦相似度
			docIdByWeight.push_back(make_pair(docId,cosSamilarity));
		}
		std::sort(docIdByWeight.begin(),docIdByWeight.end(),compareByWeight);//按相似度排序
		vector<int> retDocs;//返回的排序的文档id
		for(auto & elem:docIdByWeight){
			retDocs.push_back(elem.first);
		}
		return retDocs;
	}

	string WordQuery::doQuery(const string & str){//执行查询，返回结果
		auto confMap = _conf.getConfMap();
		auto stopWords = _conf.getStopWordList();
		//分词
		vector<string> queryWords = WordSegmentation::getInstance().cutStr(str);
		//去停用词
		for(auto it = queryWords.begin();it != queryWords.end();){
			if(stopWords.count(*it)){
				it = queryWords.erase(it);
			}else{
				++it;
			}
		}
		//查找包含单词的文档
		//格式：map<docId,set<pair<wordId,weight> > >,wordId是指word在queryWords的下标,weight是指word在docId的网页中的权重
		map<int,set<pair<int,double> > >  docsOfWords;//map<docId,set<pair<wordId,weight> > >
		for(size_t i = 0;i < queryWords.size();++i){
			string word = queryWords[i];//单词
			if(_invertIndexTable.count(word)){
				set< pair <int,double> >  docIdAndWeights = _invertIndexTable[word];
				for(auto & key:docIdAndWeights){
					int docId = key.first;//文档Id
					double weight = key.second;//单词在docId文档中的权重
					if(docsOfWords.count(docId)){
						docsOfWords[docId].insert(make_pair(i,weight));
					}else{
						docsOfWords[docId] = set<pair<int,double>> {{docId,weight}};
					}
				}
			}
		}
		//找到包含所有的查词单词的文档
		vector<pair<int,vector<double> > > resultVec;
		for(auto & elem:docsOfWords){
			if(elem.second.size() == queryWords.size()){
				int docId = elem.first;
				vector<double> weights;
				for(auto & w:elem.second){//获得权重
					weights.push_back(w.second);
				}
				resultVec.push_back(make_pair(docId,weights));
			}
		}

		//得到排序的文档id
		vector<int> docIdVec;
		string ret;
		if(!resultVec.empty()){
			docIdVec = executeQuery(queryWords,resultVec);
			ret = createJson(docIdVec,queryWords);
			return ret;
		}else{
			return returnNoAnswer();
		}
	}

	string WordQuery::createJson(vector<int> & docIdVec,const vector<string> & queryWords){
		Json::Value root;
		Json::Value docments;
		int cnt = 0;
		for(auto & docId:docIdVec){
			string summary = _pageLib[docId].getSummary(queryWords);
			string title = _pageLib[docId].getTitle();
			string url = _pageLib[docId].getUrl();
			Json::Value doc;
			doc["title"] = title;
			doc["summary"] = summary;
			doc["url"] = url;
			docments.append(doc);
			++cnt;
			if(cnt >= 100){//最多记录100条
				break;
			}
		}
		root["files"] = docments;
		Json::StyledWriter writer;
		return writer.write(root);
	}

	string WordQuery::returnNoAnswer(){
		Json::Value root;
		Json::Value documents;
		Json::Value elem;
		elem["title"] = "404, not found";
		elem["summary"] = "sorry，没有查询到你想要的信息!";
		elem["url"] = "https://www.baidu.com";
		documents.append(elem);
		root["files"] = documents;
		Json::StyledWriter writer;
		return writer.write(root);
	}
};//end of namespace
