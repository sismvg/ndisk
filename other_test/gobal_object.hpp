#ifndef ISU_GOBAL_OBJECT_HPP
#define ISU_GOBAL_OBJECT_HPP

#include <map>
#include <string>

#include <boost/thread/mutex.hpp>

class objects
{
public:
	template<class T>
	void insert_object(const std::string& name, T& object)
	{
		auto& pair = _objects[name];
		pair.second = reinterpret_cast<void*>(&object);
	}

	typedef boost::mutex lock_type;

	template<class T>
	std::pair<lock_type*, T*> get_object(const std::string& name)
	{
		auto& value = _objects[name];
		return std::make_pair(&value.first,
			reinterpret_cast<T*>(value.second));
	}

private:
	std::map <
		std::string, std::pair < lock_type, void* >> _objects;
};

extern objects objs;
#endif