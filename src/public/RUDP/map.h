//
//  map.h
//  RUDP
//
//  Created by Timothy Smale on 2016/03/28.
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#ifndef RUDP_map_h
#define RUDP_map_h

#include <RUDP/util.h>
#include <RUDP/nodestore.h>
#include <stdint.h>
#include <vector>

namespace RUDP
{
    template <typename Type>
    struct MapEntry
    {
        Type m_obj;
        MapEntry *m_next;
        uint32_t m_hash;
        
        MapEntry() : m_next(NULL), m_hash(0) {}
    };
    
    template <typename Type>
    class Map
    {
    private:
        std::vector<MapEntry<Type>*> m_buckets;
        uint32_t m_numEntries;
        
    public:
        Map(uint32_t numEntries) : m_numEntries(0)
        {
            m_buckets.resize(numEntries);
        }
        
        ~Map()
        {
            for (size_t i = 0; i < m_buckets.size(); i++)
            {
                RUDP::MapEntry<Type> **bucket = &m_buckets[i];
                while (*bucket)
                {
                    RUDP::MapEntry<Type> *node = *bucket;
                    *bucket = node->m_next;
                    RUDP::NodeStore<RUDP::MapEntry<Type>>::free(node);
                }
            }
        }
        
        bool remove(Type *key)
        {
            if (m_buckets.size() > 0)
            {
                uint32_t hash = key->hash();
                int index = hash % m_buckets.size();
                
                RUDP::MapEntry<Type> *prev = NULL;
                for (RUDP::MapEntry<Type> *entry = m_buckets[index]; entry != NULL; entry = entry->m_next)
                {
                    if (entry->m_hash == hash && entry->m_obj.equals(key))
                    {
                        if (prev == NULL)
                        {
                            m_buckets[index] = entry->m_next;
                        }
                        else
                        {
                            prev->m_next = entry->m_next;
                        }
                        
                        RUDP::NodeStore<RUDP::MapEntry<Type>>::free(entry);
                        m_numEntries--;
                        return true;
                    }
                    
                    prev = entry;
                }
            }
            
            return false;
        }
        
        Type *insert(Type *value)
        {
            if (m_numEntries >= m_buckets.size())
            {
                return NULL;
            }
            
            // hashmap insert
            RUDP::MapEntry<Type> *entry = (RUDP::MapEntry<Type>*)RUDP::NodeStore<RUDP::MapEntry<Type>>::secure();
            if (!entry)
            {
                return NULL;
            }
            
            m_numEntries++;
            
            entry->m_next = NULL;
            entry->m_obj = *value;
            entry->m_hash = value->hash();
            
            size_t index = entry->m_hash % m_buckets.size();
            if (m_buckets[index] == NULL)
            {
                m_buckets[index] = entry;
            }
            else
            {
                for (RUDP::MapEntry<Type> *bucket = m_buckets[index]; bucket != NULL; bucket = bucket->m_next)
                {
                    if (bucket->m_next == NULL)
                    {
                        bucket->m_next = entry;
                        break;
                    }
                }
            }
            
            return &entry->m_obj;
        }
        
        Type *find(Type *key)
        {
            uint32_t hash = key->hash();
            int index = hash % m_buckets.size();
            
            for (RUDP::MapEntry<Type> *entry = m_buckets[index]; entry != NULL; entry = entry->m_next)
            {
                if (entry->m_hash == hash && entry->m_obj.equals(key))
                {
                    return &entry->m_obj;
                }
            }
            
            return NULL;
        }
    };
}

#endif