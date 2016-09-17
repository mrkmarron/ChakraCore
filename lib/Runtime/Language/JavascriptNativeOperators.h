//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
namespace Js
{
#if ENABLE_NATIVE_CODEGEN
    template <typename T>
    class BranchDictionaryWrapper
    {
    public:
        class DictAllocator :public NativeCodeData::Allocator
        {
        public:
            char * Alloc(size_t requestedBytes)
            {
                char* dataBlock = __super::Alloc(requestedBytes);
#if DBG
                NativeCodeData::DataChunk* chunk = NativeCodeData::GetDataChunk(dataBlock);
                chunk->dataType = "BranchDictionary::Bucket";
                if (PHASE_TRACE1(Js::NativeCodeDataPhase))
                {
                    Output::Print(L"NativeCodeData BranchDictionary::Bucket: chunk: %p, data: %p, index: %d, len: %x, totalOffset: %x, type: %S\n",
                        chunk, (void*)dataBlock, chunk->allocIndex, chunk->len, chunk->offset, chunk->dataType);
                }
#endif
                return dataBlock;
            }

            char * AllocZero(size_t requestedBytes)
            {
                char* dataBlock = __super::AllocZero(requestedBytes);
#if DBG
                NativeCodeData::DataChunk* chunk = NativeCodeData::GetDataChunk(dataBlock);
                chunk->dataType = "BranchDictionary::Entries";
                if (PHASE_TRACE1(Js::NativeCodeDataPhase))
                {
                    Output::Print(L"NativeCodeData BranchDictionary::Entries: chunk: %p, data: %p, index: %d, len: %x, totalOffset: %x, type: %S\n",
                        chunk, (void*)dataBlock, chunk->allocIndex, chunk->len, chunk->offset, chunk->dataType);
                }
#endif
                return dataBlock;
            }
        };

        template <class TKey, class TValue>
        class SimpleDictionaryEntryWithFixUp : public JsUtil::SimpleDictionaryEntry<TKey, TValue>
        {
        public:
            __declspec(noinline)
            void FixupWithRemoteKey(void* remoteKey)
            {
                this->key = (TKey)remoteKey;
            }
        };       

        typedef JsUtil::BaseDictionary<T, void*, DictAllocator, PowerOf2SizePolicy, DefaultComparer, SimpleDictionaryEntryWithFixUp> BranchBaseDictionary;

        class BranchDictionary :public BranchBaseDictionary
        {
        public:
            BranchDictionary(DictAllocator* allocator, uint dictionarySize)
                : BranchBaseDictionary(allocator, dictionarySize)
            {
            }
            __declspec(noinline)
            void Fixup(NativeCodeData::DataChunk* chunkList, void** remoteKeys)
            {
                for (int i = 0; i < this->Count(); i++)
                {
                    this->entries[i].FixupWithRemoteKey(remoteKeys[i]);
                }
                FixupNativeDataPointer(buckets, chunkList);
                FixupNativeDataPointer(entries, chunkList);
            }
        };

        BranchDictionaryWrapper(NativeCodeData::Allocator * allocator, uint dictionarySize, ArenaAllocator* remoteKeyAlloc) :
            defaultTarget(nullptr), dictionary((DictAllocator*)allocator, dictionarySize)
        {
            if (remoteKeyAlloc)
            {
                remoteKeys = AnewArrayZ(remoteKeyAlloc, void*, dictionarySize);
            }
            else
            {
                remoteKeys = nullptr;
            }
        }

        BranchDictionary dictionary;
        void* defaultTarget;
        void** remoteKeys;

        bool IsOOPJit()
        {
            return remoteKeys != nullptr;
        }

        static BranchDictionaryWrapper* New(NativeCodeData::Allocator * allocator, uint dictionarySize, ArenaAllocator* remoteKeyAlloc)
        {
            return NativeCodeDataNew(allocator, BranchDictionaryWrapper, allocator, dictionarySize, remoteKeyAlloc);
        }

        void AddEntry(uint32 offset, T key, void* remoteVar)
        {
            int index = dictionary.AddNew(key, (void**)offset);
            if (IsOOPJit())
            {
                remoteKeys[index] = remoteVar;
            }
        }

        void Fixup(NativeCodeData::DataChunk* chunkList)
        {
            if (IsOOPJit())
            {
                dictionary.Fixup(chunkList, remoteKeys);
            }
        }
    };

    class JavascriptNativeOperators
    {
    public:
        static void * Op_SwitchStringLookUp(JavascriptString* str, Js::BranchDictionaryWrapper<Js::JavascriptString*>* stringDictionary, uintptr_t funcStart, uintptr_t funcEnd);
    };
#endif
};
