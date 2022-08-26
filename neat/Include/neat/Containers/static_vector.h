
#pragma once

#include "neat/defines.h"
#include <assert.h>
#include <initializer_list>

using long_int = long long int;

namespace neat
{
	template<typename t>
	struct reverse
	{
		t* rbegin;
		t* rend;
		std::reverse_iterator<t*> begin()
		{
			return std::reverse_iterator<t*>(rbegin);
		}
		std::reverse_iterator<t*> end()
		{
			return std::reverse_iterator<t*>(rend);
		}
	};

	template<typename t, int width>
	class static_vector
	{
	public:
		typedef t value_type;
		
		static_vector();
		static_vector(std::initializer_list<t> init_list);
		static_vector(const static_vector<t, width>& copy_from);
		static_vector& operator=(const static_vector<t, width>& copy_from);
		static_vector(static_vector<t, width>&& move_from) noexcept;
		static_vector& operator=(static_vector<t, width>&& move_from) noexcept;
		~static_vector();

		template< typename ... args >
		bool emplace_back(args&& ... params);
		template< typename ... args >
		bool emplace_front(args&& ... params);

		bool append(const static_vector<t, width>& other_vector);

		bool push_back(t&& item);
		bool push_front(t&& item);

		bool erase(t* item);
		bool erase(int index);
		void clear();

		void resize(unsigned new_size);
		void resize(unsigned new_size, t value);

		unsigned int size();
		unsigned int size() const;
		bool empty();
		bool empty() const;

		unsigned int max_size() const;

		t* data();
		t* data() const;
		t& operator[](int index);
		const t& operator[](int index) const;

		reverse<t> reverse_iterate();

		t& front()
		{
			return _buffer[0];
		}
		t& back()
		{
			return _buffer[(_size - 1) * !empty()];
		}
		t* begin()
		{
			return _buffer;
		}
		t* end()
		{
			return &_buffer[_size];
		}
		t* begin() const
		{
			return _buffer;
		}
		t* end() const
		{
			return &_buffer[_size];
		}

	private:
		t* _buffer = nullptr;
		int _size = 0;

	};

	template<typename t, int width>
	inline static_vector<t, width>::static_vector()
	{
		//static_assert(std::is_trivially_copyable<t>::value && "static_vector only supports trivially copiable types");
		_buffer = (t*)malloc(width * sizeof t);
	}

	template<typename t, int width>
	inline static_vector<t, width>::static_vector(std::initializer_list<t> init_list) : static_vector()
	{
		int size = 0;
		for (int i = 0; i < init_list.size() && i < width; ++i)
		{
			_buffer[i] = init_list.begin()[i];
			size++;
		}
		_size = size;
	}

	template<typename t, int width>
	inline static_vector<t, width>::static_vector(const static_vector<t, width>& copy_from) : static_vector()
	{
		memcpy(_buffer, copy_from._buffer, width * sizeof t);
		_size = copy_from._size;
	}

	template <typename t, int width>
	static_vector<t, width>& static_vector<t, width>::operator=(const static_vector<t, width>& copy_from)
	{
		memcpy(_buffer, copy_from._buffer, width * sizeof t);
		_size = copy_from._size;
		return *this;
	}

	template<typename t, int width>
	inline static_vector<t, width>::static_vector(static_vector<t, width>&& move_from) noexcept
		: _buffer(std::exchange(move_from._buffer, nullptr))
		, _size(std::exchange(move_from._size, 0))
	{
	}

	template <typename t, int width>
	static_vector<t, width>& static_vector<t, width>::operator=(static_vector<t, width>&& move_from) noexcept
	{
		_buffer = std::exchange(move_from._buffer, nullptr);
		_size = std::exchange(move_from._size, 0);
		return *this;
	}

	template<typename t, int width>
	inline static_vector<t, width>::~static_vector()
	{
		SAFE_DELETE_ARRAY(_buffer);
	}

	template<typename t, int width>
	template< typename ... args >
	inline bool static_vector<t, width>::emplace_back(args&& ... params)
	{
		//t tmp(params...);
		/*
		const bool valid_op = ( this_back < width - 1 );

		const size_t copy_size = valid_op * sizeof( t );
		this_back += valid_op;
		memcpy( &this_buffer[this_back], &tmp, copy_size );*/

		return push_back(t(params...));
	}

	template<typename t, int width>
	template< typename ... args >
	inline bool static_vector<t, width>::emplace_front(args&& ... params)
	{
		//t tmp(params...);
		/*const bool valid_op = ( this_back < width - 1 );

		const size_t push_size = valid_op * sizeof( t ) * ( this_back + 1 );
		memmove( &this_buffer[1], this_buffer, push_size );

		const size_t copy_size = valid_op * sizeof( t );
		memcpy( &this_buffer[0], &params, copy_size );

		this_back += valid_op;*/

		return push_front(t(params...));
	}

	template<typename t, int width>
	inline bool static_vector<t, width>::append(const static_vector<t, width>& other_vector)
	{
		size_t szLeft = width - _size;
		size_t copy_size = szLeft < other_vector.size() ? szLeft : other_vector.size();

		memcpy(&_buffer[_size], &other_vector.data(), copy_size * sizeof t);
		_size += copy_size;
		
		return !!copy_size;
	}

	template<typename t, int width>
	inline bool static_vector<t, width>::push_back(t&& item)
	{
		const bool valid_op = (_size < width);

		const size_t copy_size = valid_op * sizeof(t);
		memcpy(&_buffer[_size], &item, copy_size);
		_size += valid_op;

		return !!copy_size;
	}

	template<typename t, int width>
	inline bool static_vector<t, width>::push_front(t&& item)
	{
		const bool valid_op = (_buffer < width);

		const size_t push_size = valid_op * sizeof(t) * (_size);
		memmove(&_buffer[1], _buffer, push_size);

		const size_t copy_size = valid_op * sizeof(t);
		memcpy(&_buffer[0], &item, copy_size);

		_size += valid_op;

		return !!copy_size || !!push_size;
	}

	template<typename t, int width>
	inline bool static_vector<t, width>::erase(t* item)
	{
		const int index = item - _buffer;
		return erase(index);
	}

	template<typename t, int width>
	inline bool static_vector<t, width>::erase(int index)
	{
		const bool valid_op = (index > -1) & (index < _size);
		memcpy(_buffer + index, _buffer + (_size - 1), sizeof(t) * valid_op);

		_size -= valid_op;
		return valid_op;
	}

	template<typename t, int width>
	inline void static_vector<t, width>::clear()
	{
		_size = 0;
	}

	template<typename t, int width>
	inline void static_vector<t, width>::resize(unsigned new_size)
	{
		_size = new_size;

	}

	template<typename t, int width>
	inline void static_vector<t, width>::resize(unsigned new_size, t value)
	{
		int old_size = _size;
		_size = new_size;
		for (int i = old_size; i < new_size; ++i)
		{
			_buffer[i] = value;
		}
	}

	template<typename t, int width>
	inline unsigned int static_vector<t, width>::size()
	{
		return _size;
	}

	template<typename t, int width>
	inline unsigned int static_vector<t, width>::size() const
	{
		return _size;
	}

	template<typename t, int width>
	inline bool static_vector<t, width>::empty()
	{
		return _size == 0;
	}

	template<typename t, int width>
	inline bool static_vector<t, width>::empty() const
	{
		return _size == 0;
	}

	template<typename t, int width>
	inline unsigned int static_vector<t, width>::max_size() const
	{
		return width;
	}

	template<typename t, int width>
	inline t* static_vector<t, width>::data()
	{
		return _buffer;
	}

	template<typename t, int width>
	inline t* static_vector<t, width>::data() const
	{
		return _buffer;
	}

	template <typename t, int width>
	t& static_vector<t, width>::operator[](int index)
	{
		const bool valid_op = index > -1 & index < _size;
		assert(valid_op && "index out of range");
		return _buffer[index];
	}

	template <typename t, int width>
	const t& static_vector<t, width>::operator[](int index) const
	{
		const bool valid_op = index > -1 & index < _size;
		assert(valid_op && "index out of range");
		return _buffer[index];
	}
	template<typename t, int width>
	inline reverse<t> static_vector<t, width>::reverse_iterate()
	{
		return { _buffer + _size, _buffer };
	}


}
