//
//  queue.h
//  RUDP
//
//  Created by Timothy Smale on 2016/03/28.
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#ifndef RUDP_queue_h
#define RUDP_queue_h

#include <stdint.h>
#include <RUDP/util.h>

namespace RUDP
{
	template <typename Type>
	struct Node
	{
		Type m_obj;
		Node *m_next;
		Node *m_prev;
		bool m_active;

		Node() : m_next(NULL), m_prev(NULL), m_active(false) {}
	};

	template <typename Type>
	class NodeStore
	{
	public:
		
	private:
		RUDP::Node<Type> *m_nodes;
		RUDP::Node<Type> *m_nodeFreeList;
		size_t m_numUsed;
		const size_t m_max = UINT16_MAX;

	public:
		bool isValid(void *obj)
		{
			return (void*)obj >= (void*)m_nodes && (void*)obj < (void*)(m_nodes + m_max);
		}

		bool hasSpace()
		{
			return m_numUsed < m_max;
		}

		size_t getNumTotal()
		{
			return m_max;
		}

		size_t getNumSecured()
		{
			return m_numUsed;
		}

		NodeStore() : m_nodes(NULL), m_nodeFreeList(NULL), m_numUsed(0)
		{
			m_nodes = new RUDP::Node<Type>[m_max];
			m_nodeFreeList = m_nodes;

			for (size_t i = 0; i < m_max; i++)
			{
				m_nodes[i].m_active = false;

				if (i == m_max - 1)
				{
					m_nodes[i].m_next = NULL;
					m_nodes[i].m_prev = &m_nodes[i - 1];
				}
				else if (i == 0)
				{
					m_nodes[i].m_next = &m_nodes[i + 1];
					m_nodes[i].m_prev = NULL;
				}
				else
				{
					m_nodes[i].m_next = &m_nodes[i + 1];
					m_nodes[i].m_prev = &m_nodes[i - 1];
				}
			}
		}

		void free(Type *obj)
		{
			RUDP::Node<Type> *node = (RUDP::Node<Type>*)obj;

			if (isValid(node))
			{
				node->m_active = false;
				node->m_next = m_nodeFreeList;
				m_nodeFreeList = node;
				m_numUsed--;

				//printf("%p pop %.*s %d\n\n", this, obj->getUserDataSize(), obj->getUserDataPtr(), m_num);
			}
		}

		Type *secure()
		{
			if (m_numUsed < m_max)
			{
				RUDP::Node<Type> *node = m_nodeFreeList;
				m_nodeFreeList = node->m_next;

				node->m_obj = Type();
				node->m_next = NULL;
				node->m_prev = NULL;
				node->m_active = true;
				m_numUsed++;

				//printf("%p push %.*s %d\n\n", this, obj->getUserDataSize(), obj->getUserDataPtr(), m_num);

				return &node->m_obj;
			}

			return NULL;
		}

		~NodeStore()
		{
			delete[] m_nodes;
		}
	};

	template <typename Type>
	class Queue
	{
	private:
		RUDP::Node<Type> *m_head;
		RUDP::Node<Type> *m_end;

	public:
		Queue() : m_head(NULL), m_end(NULL) {}

		Type *peek()
		{
			if (m_end)
			{
				return &m_end->m_obj;
			}

			return NULL;
		}

		Type *next(Type *obj)
		{
			RUDP::Node<Type> *node = (RUDP::Node<Type>*)obj;
			if (node->m_active && node->m_next && node->m_next->m_active)
			{
				return &node->m_next->m_obj;
			}

			return NULL;
		}

		Type *pop()
		{
			if (m_head)
			{
				if (m_head == m_end)
				{
					m_end = NULL;
				}

				RUDP::Node<Type> *node = m_head;
				Type *obj = &node->m_obj;
				m_head = node->m_next;
				
				if (m_head)
				{
					m_head->m_prev = NULL;
				}

				return obj;
			}

			return NULL;
		}

		Type *remove(Type* obj)
		{
			
			RUDP::Node<Type> *node = (RUDP::Node<Type>*)obj;
			if (node->m_active)
			{
				if (node->m_prev)
				{
					node->m_prev->m_next = node->m_next;
				}

				if (node->m_next)
				{
					node->m_next->m_prev = node->m_prev;
				}

				if (node == m_head)
				{
					if (m_head == m_end)
					{
						m_end = NULL;
					}

					m_head = node->m_next;
				}
				else if (node == m_end)
				{
					m_end = node->m_prev;
				}
					
				return (Type*)node->m_prev;
			}

			return NULL;
		}

		void push(Type* obj)
		{
			RUDP::Node<Type> *node = (RUDP::Node<Type>*)obj;

			if (m_end)
			{
				node->m_prev = m_end;
				m_end->m_next = node;
			}
			else
			{
				node->m_prev = NULL;
				m_head = node;
			}

			m_end = node;
		}

		void pushAfter(Type *after, Type *obj)
		{
			RUDP::Node<Type> *afterNode = (RUDP::Node<Type>*)after;
			RUDP::Node<Type> *objNode = (RUDP::Node<Type>*)obj;

			objNode->m_next = afterNode->m_next;
			objNode->m_prev = afterNode;

			if (afterNode->m_next)
			{
				afterNode->m_next->m_prev = objNode;
			}

			afterNode->m_next = objNode;
		}
	};
}

#endif