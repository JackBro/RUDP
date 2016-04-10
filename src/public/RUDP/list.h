//
//  list.h
//  RUDP
//
//  Created by Timothy Smale on 2016/03/28.
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#ifndef RUDP_list_h
#define RUDP_list_h

#include <stdint.h>
#include <RUDP/nodestore.h>
#include <RUDP/util.h>
#include <stdlib.h>

namespace RUDP
{
    template <typename Type>
    class List
    {
    private:
        RUDP::Node<Type> *m_head;
        RUDP::Node<Type> *m_end;
        
    public:
        List() : m_head(NULL), m_end(NULL) {}
        
        ~List()
        {
            free();
        }
        
        void free()
        {
            while (m_head)
            {
                RUDP::Node<Type> *node = m_head;
                m_head = node->m_next;
                RUDP::NodeStore<Type>::free(node);
            }
            
            m_end = NULL;
        }
        
        void inheritFrom(RUDP::List<Type> *other)
        {
            if(!other->m_head)
            {
                return;
            }
            
            if (!m_end)
            {
                m_head = other->m_head;
                m_end = other->m_end;
            }
            else
            {
                if(other->m_head)
                {
                    other->m_head->m_prev = m_end;
                }
                
                if(m_end->m_next)
                {
                    m_end->m_next->m_prev = other->m_head;
                }
                
                m_end->m_next = other->m_head;
                m_end = other->m_end;
            }
            
            other->m_head = NULL;
            other->m_end = NULL;
        }
        
        Type *peek()
        {
            if (m_head)
            {
                return &m_head->m_obj;
            }
            
            return NULL;
        }
        
        Type *peekEnd()
        {
            if (m_end)
            {
                return &m_end->m_obj;
            }
            
            return NULL;
        }
        
        Type *next(Type *obj)
        {
            if (RUDP::NodeStore<Type>::isValid(obj))
            {
                RUDP::Node<Type> *node = (RUDP::Node<Type>*)obj;
                if (node->m_active && node->m_next && node->m_next->m_active)
                {
                    return &node->m_next->m_obj;
                }
            }
            
            return NULL;
        }
        
        Type *prev(Type *obj)
        {
            if (RUDP::NodeStore<Type>::isValid(obj))
            {
                RUDP::Node<Type> *node = (RUDP::Node<Type>*)obj;
                if (node->m_active && node->m_prev && node->m_prev->m_active)
                {
                    return &node->m_prev->m_obj;
                }
            }
            
            return NULL;
        }
        
        bool pop()
        {
            if (m_head)
            {
                if (m_head == m_end)
                {
                    m_end = NULL;
                }
                
                RUDP::Node<Type> *node = m_head;
                m_head = node->m_next;
                
                if (m_head)
                {
                    m_head->m_prev = NULL;
                }
                
                RUDP::NodeStore<Type>::free(node);
                return true;
            }
            
            return false;
        }
        
        void remove(Type* obj)
        {
            if (RUDP::NodeStore<Type>::isValid(obj))
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
                    
                    RUDP::NodeStore<Type>::free(node);
                }
            }
        }
        
        Type *push(Type* obj = NULL)
        {
            if (m_end)
            {
                return pushAfter((Type*)m_end, obj);
            }
            else
            {
                RUDP::Node<Type> *objNode = RUDP::NodeStore<Type>::secure();
                if (!objNode)
                {
                    return NULL;
                }
                
                if (obj)
                {
                    objNode->m_obj = *obj;
                }
                
                m_end = objNode;
                m_head = objNode;
                return &objNode->m_obj;
            }
        }
        
        Type *pushAfter(Type *after, Type *obj = NULL)
        {
            if (RUDP::NodeStore<Type>::isValid(after))
            {
                RUDP::Node<Type> *afterNode = (RUDP::Node<Type>*)after;
                RUDP::Node<Type> *objNode = RUDP::NodeStore<Type>::secure();
                
                if (obj)
                {
                    objNode->m_obj = *obj;
                }
                
                objNode->m_next = afterNode->m_next;
                objNode->m_prev = afterNode;
                
                if (afterNode->m_next)
                {
                    afterNode->m_next->m_prev = objNode;
                }
                
                if (afterNode == m_end)
                {
                    m_end = objNode;
                }
                
                afterNode->m_next = objNode;
                return &objNode->m_obj;
            }
            
            return NULL;
        }
        
        Type *pushBefore(Type *before, Type *obj)
        {
            if (RUDP::NodeStore<Type>::isValid(before))
            {
                RUDP::Node<Type> *beforeNode = (RUDP::Node<Type>*)before;
                RUDP::Node<Type> *objNode = RUDP::NodeStore<Type>::secure();
                
                if (obj)
                {
                    objNode->m_obj = *obj;
                }
                
                objNode->m_prev = beforeNode->m_prev;
                objNode->m_next = beforeNode;
                
                if (beforeNode->m_prev)
                {
                    beforeNode->m_prev->m_next = objNode;
                }
                
                if (beforeNode == m_head)
                {
                    m_head = objNode;
                }
                
                beforeNode->m_prev = objNode;
                return &objNode->m_obj;
            }
            
            return NULL;
        }
    };
}

#endif