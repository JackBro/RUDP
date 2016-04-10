//
//  nodestore.h
//  RUDP
//
//  Created by Timothy Smale on 2016/03/28.
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#ifndef RUDP_nodestore_h
#define RUDP_nodestore_h

#include <stdlib.h>
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
        
        Node() : m_next(NULL), m_prev(NULL), m_active(false), m_obj(Type()) {}
    };
    
    template <typename Type>
    class NodeStore
    {
    private:
        static RUDP::Node<Type> *s_nodes;
        static RUDP::Node<Type> *s_nodeFreeList;
        static size_t s_numUsed;
        static size_t s_max;
        static std::mutex s_lock;
        static bool s_initialized;
        
    public:
        static bool isValid(void *obj)
        {
            return (void*)obj >= (void*)s_nodes && (void*)obj < (void*)(s_nodes + s_max);
        }
        
        static bool hasSpace()
        {
            return s_numUsed < s_max;
        }
        
        static size_t getNumTotal()
        {
            return s_max;
        }
        
        static size_t getNumSecured()
        {
            return s_numUsed;
        }
        
        static void initialize(size_t max)
        {
            if (!s_initialized)
            {
                s_lock.lock();
                
                if(!s_initialized)
                {
                    s_max = max;
                    s_nodes = new RUDP::Node<Type>[s_max];
                    s_nodeFreeList = s_nodes;
                    s_numUsed = 0;
                    
                    for (size_t i = 0; i < s_max; i++)
                    {
                        s_nodes[i].m_active = false;
                        
                        if (i == s_max - 1)
                        {
                            s_nodes[i].m_next = NULL;
                            s_nodes[i].m_prev = &s_nodes[i - 1];
                        }
                        else if (i == 0)
                        {
                            s_nodes[i].m_next = &s_nodes[i + 1];
                            s_nodes[i].m_prev = NULL;
                        }
                        else
                        {
                            s_nodes[i].m_next = &s_nodes[i + 1];
                            s_nodes[i].m_prev = &s_nodes[i - 1];
                        }
                    }
                    
                    s_initialized = true;
                }
                
                s_lock.unlock();
            }
        }
        
        static void free(Type *node)
        {
            free((RUDP::Node<Type>*)node);
        }
        
        static void free(RUDP::Node<Type> *node)
        {
            if (isValid(node))
            {
                s_lock.lock();
                
                node->m_active = false;
                node->m_next = s_nodeFreeList;
                s_nodeFreeList = node;
                s_numUsed--;
                
                //RUDP_PRINTF("%p pop %.*s %d\n\n", this, obj->getUserDataSize(), obj->getUserDataPtr(), m_num);
                
                s_lock.unlock();
            }
        }
        
        static RUDP::Node<Type> *secure()
        {
            initialize(256);
            
            if (s_numUsed < s_max)
            {
                s_lock.lock();
                
                RUDP::Node<Type> *node = s_nodeFreeList;
                s_nodeFreeList = node->m_next;
                
                node->m_obj = Type();
                node->m_next = NULL;
                node->m_prev = NULL;
                node->m_active = true;
                s_numUsed++;
                
                //RUDP_PRINTF("%p push %.*s %d\n\n", this, obj->getUserDataSize(), obj->getUserDataPtr(), m_num);
                
                s_lock.unlock();
                
                return node;
            }
            
            return NULL;
        }
        
        static void deinitialize()
        {
            delete[] s_nodes;
        }
    };
    
    template <typename Type>
    RUDP::Node<Type> *RUDP::NodeStore<Type>::s_nodes = NULL;
    
    template <typename Type>
    RUDP::Node<Type> *RUDP::NodeStore<Type>::s_nodeFreeList = NULL;
    
    template <typename Type>
    size_t RUDP::NodeStore<Type>::s_numUsed = 0;
    
    template <typename Type>
    size_t RUDP::NodeStore<Type>::s_max = 0;
    
    template <typename Type>
    bool RUDP::NodeStore<Type>::s_initialized = false;
    
    template <typename Type>
    std::mutex RUDP::NodeStore<Type>::s_lock;
}

#endif