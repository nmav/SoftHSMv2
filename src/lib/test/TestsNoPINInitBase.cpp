/*
 * Copyright (c) 2010 .SE (The Internet Infrastructure Foundation)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*****************************************************************************
 TestsNoPINInitBase.cpp

 Base class for test classes. Used when there is no need for user login.
 *****************************************************************************/

#include "TestsNoPINInitBase.h"
#include <cstring>
#include <cppunit/extensions/HelperMacros.h>
#include <vector>
#include <sstream>

#ifdef _WIN32
CK_FUNCTION_LIST_PTR FunctionList::getFunctionListPtr(const char*const libName,  HINSTANCE__* p11Library, const char*getFunctionList) {
#else
#include <dlfcn.h>

static CK_FUNCTION_LIST_PTR getFunctionListPtr(const char*const libName, void *const p11Library, const char*getFunctionList) {
#endif
	CPPUNIT_ASSERT_MESSAGE(libName, p11Library);
#ifdef _WIN32
	const CK_C_GetFunctionList pGFL( (CK_C_GetFunctionList)GetProcAddress(
			p11Library,
			getFunctionList.c_str()
	) );
#else
	const CK_C_GetFunctionList pGFL( (CK_C_GetFunctionList)dlsym(
			p11Library,
			getFunctionList
	) );
#endif
	CPPUNIT_ASSERT_MESSAGE(libName, pGFL);
	CK_FUNCTION_LIST_PTR ptr(NULL_PTR);
	const CK_RV retCode( pGFL(&ptr) );
	if ( !ptr && (retCode)!=CKR_OK) {
		std::ostringstream oss;
		oss << "C_GetFunctionList failed...error no = 0x" << std::hex << retCode << " libName '" << libName << "'.";
		CPPUNIT_ASSERT_MESSAGE(oss.str(), false);
	}
	return ptr;
}
static void getSlotIDs( CK_SLOT_ID*const pInitializedTokenSlotID, CK_SLOT_ID*const pFreeTokenSlotID ) {
	bool hasFoundFree(false);
	bool hasFoundInitialized(false);
	CK_ULONG nrOfSlots;
	CPPUNIT_ASSERT( CRYPTOKI_F_PTR( C_GetSlotList(CK_TRUE, NULL_PTR, &nrOfSlots)==CKR_OK ) );
	std::vector<CK_SLOT_ID> slotIDs(nrOfSlots);
	CPPUNIT_ASSERT( CRYPTOKI_F_PTR( C_GetSlotList(CK_TRUE, &slotIDs.front(), &nrOfSlots)==CKR_OK ) );
	for ( std::vector<CK_SLOT_ID>::iterator i=slotIDs.begin(); i!=slotIDs.end(); i++ ) {
		CK_TOKEN_INFO tokenInfo;
		CPPUNIT_ASSERT( CRYPTOKI_F_PTR( C_GetTokenInfo(*i, &tokenInfo)==CKR_OK ) );
		if ( tokenInfo.flags&CKF_TOKEN_INITIALIZED ) {
			if ( !hasFoundInitialized ) {
				hasFoundInitialized = true;
				*pInitializedTokenSlotID = *i;
			}
		} else {
			if ( !hasFoundFree ) {
				hasFoundFree = true;
				*pFreeTokenSlotID = *i;
			}
		}
	}
	if ( !hasFoundInitialized ) {
		*pInitializedTokenSlotID = *pFreeTokenSlotID;
	}
}

TestsNoPINInitBase::TestsNoPINInitBase() :
#ifdef P11M
#ifdef _WIN32
		p11Library( LoadLibrary(libName.c_str()) ),
#else
		p11Library( dlopen(P11M, RTLD_LAZY) ),
#endif
		ptr(getFunctionListPtr(P11M, p11Library, "C_GetFunctionList")),
#endif
		m_invalidSlotID(-1),
		m_initializedTokenSlotID(m_invalidSlotID),
		m_notInitializedTokenSlotID(m_invalidSlotID),
		m_soPin1((CK_UTF8CHAR_PTR)"12345678"),
		m_soPin1Length(strlen((char*)m_soPin1)),
		m_userPin1((CK_UTF8CHAR_PTR)"1234"),
		m_userPin1Length(strlen((char*)m_userPin1)) {};

void TestsNoPINInitBase::setUp() {
	CK_UTF8CHAR label[32];
	memset(label, ' ', 32);
	memcpy(label, "token1", strlen("token1"));

	// initialize cryptoki
	CPPUNIT_ASSERT( CRYPTOKI_F_PTR( C_Initialize(NULL_PTR)==CKR_OK ) );
	// update slot IDs to initialized and not initialized token.
	getSlotIDs(&m_initializedTokenSlotID, &m_notInitializedTokenSlotID);
	// (Re)initialize the token
	CPPUNIT_ASSERT( CRYPTOKI_F_PTR( C_InitToken(m_initializedTokenSlotID, m_soPin1, m_soPin1Length, label)==CKR_OK ) );
	// Reset cryptoki to get new slot IDs.
	CPPUNIT_ASSERT( CRYPTOKI_F_PTR( C_Finalize(NULL_PTR)==CKR_OK ) );
	CPPUNIT_ASSERT( CRYPTOKI_F_PTR( C_Initialize(NULL_PTR)==CKR_OK ) );
	// slot IDs must be updated since the ID of the initialized token has changed.
	getSlotIDs(&m_initializedTokenSlotID, &m_notInitializedTokenSlotID);
}

void TestsNoPINInitBase::tearDown()
{
	CRYPTOKI_F_PTR( C_Finalize(NULL_PTR) );
}
TestsNoPINInitBase::~TestsNoPINInitBase() {
	if ( !p11Library ) {
		return;
	}
#ifdef _WIN32
	FreeLibrary(p11Library);
#else
	dlclose(p11Library);
#endif
}
