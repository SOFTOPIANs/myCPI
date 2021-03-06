#ifndef ESPERANTO_DATA_Q_MANAGER_H
#define ESPERANTO_DATA_Q_MANAGER_H

#include <queue>
#include "dataQElem.h"

using namespace std;

class DataQElem;

class DataQ {
public:
	DataQ();
	int getSize();
	void produce(DataQElem* data);
	DataQElem* consume();
	
private:
	std::vector<DataQElem*>* dataQ;
	pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;	
};

#endif
