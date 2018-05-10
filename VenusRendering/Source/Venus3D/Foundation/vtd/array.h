////////////////////////////////////////////////////////////////////////////
//
//  The MIT License (MIT)
//  Copyright (c) 2016 Albert D Yang
// -------------------------------------------------------------------------
//  Module:      Venus3D
//  File name:   array.h
//  Created:     2018/02/02 by Albert D Yang
//  Description:
// -------------------------------------------------------------------------
//  Permission is hereby granted, free of charge, to any person obtaining a
//  copy of this software and associated documentation files (the "Software"),
//  to deal in the Software without restriction, including without limitation
//  the rights to use, copy, modify, merge, publish, distribute, sublicense,
//  and/or sell copies of the Software, and to permit persons to whom the
//  Software is furnished to do so, subject to the following conditions:
// -------------------------------------------------------------------------
//  The above copyright notice and this permission notice shall be included
//  in all copies or substantial portions of the Software.
// -------------------------------------------------------------------------
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
//  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
//  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
//  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
//  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
//  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
////////////////////////////////////////////////////////////////////////////

#pragma once

namespace vtd
{
	template<class _Ty, size_t _Count = 16>
	class array
	{
	public:
		typedef _Ty value_type;
		typedef value_type* pointer;
		typedef const value_type* const_pointer;
		typedef value_type& reference;
		typedef const value_type& const_reference;
		typedef size_t size_type;
		typedef ptrdiff_t difference_type;

		typedef pointer iterator;
		typedef const_pointer const_iterator;

		array() noexcept = default;

		array(const value_type& _Val) noexcept
		{
			for (auto& val : buffer)
			{
				val = _Val;
			}
		}

		array(const array& _Copy) noexcept
		{
			for (size_t i(0); i < _Count; ++i)
			{
				buffer[i] = _Copy[i];
			}
		}

		array(array&& _Move) noexcept
		{
			for (size_t i(0); i < _Count; ++i)
			{
				buffer[i] = std::move(_Move[i]);
			}
		}

		array(std::initializer_list<_Ty> l) noexcept
		{
			auto it = l.begin();
			for (size_t i(0); i < _Count; ++i)
			{
				buffer[i] = *it;
				auto it_next = it + 1;
				if (it_next != l.end())
				{
					it = it_next;
				}
			}
		}

		~array() noexcept = default;

		array& operator = (const array& _Copy)
		{
			for (size_t i(0); i < _Count; ++i)
			{
				buffer[i] = _Copy[i];
			}
			return *this;
		}

		array& operator = (array&& _Move) noexcept
		{
			for (size_t i(0); i < _Count; ++i)
			{
				buffer[i] = std::move(_Move[i]);
			}
			return *this;
		}

		constexpr size_t size() const noexcept
		{
			return _Count;
		}

		iterator begin() noexcept
		{
			return buffer;
		}

		const_iterator begin() const noexcept
		{
			return buffer;
		}

		iterator end() noexcept
		{
			return buffer + size();
		}

		const_iterator end() const noexcept
		{
			return buffer + size();
		}

		const_iterator cbegin() const noexcept
		{
			return buffer;
		}

		const_iterator cend() const noexcept
		{
			return buffer + size();
		}

		reference at(size_type _Pos) noexcept
		{
			assert(_Pos < _Count);
			return buffer[_Pos];
		}

		const_reference at(size_type _Pos) const noexcept
		{
			assert(_Pos < _Count);
			return buffer[_Pos];
		}

		reference operator [] (size_type _Pos) noexcept
		{
			return at(_Pos);
		}

		const_reference operator [] (size_type _Pos) const noexcept
		{
			return at(_Pos);
		}

	private:
		value_type buffer[_Count];

	};

}
