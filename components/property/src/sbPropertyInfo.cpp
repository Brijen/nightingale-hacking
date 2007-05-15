/*
//
// BEGIN SONGBIRD GPL
//
// This file is part of the Songbird web player.
//
// Copyright(c) 2005-2007 POTI, Inc.
// http://songbirdnest.com
//
// This file may be licensed under the terms of of the
// GNU General Public License Version 2 (the "GPL").
//
// Software distributed under the License is distributed
// on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, either
// express or implied. See the GPL for the specific language
// governing rights and limitations.
//
// You should have received a copy of the GPL along with this
// program. If not, go to http://www.gnu.org/licenses/gpl.html
// or write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
// END SONGBIRD GPL
//
*/

#include "sbPropertyInfo.h"

#include <nsISimpleEnumerator.h>

#include <nsAutoLock.h>
#include <nsArrayEnumerator.h>

NS_IMPL_THREADSAFE_ISUPPORTS1(sbPropertyOperator, sbIPropertyOperator)

sbPropertyOperator::sbPropertyOperator()
: mLock(nsnull)
, mInitialized(PR_FALSE)
{
  mLock = PR_NewLock();
  NS_ASSERTION(mLock, "sbPropertyOperator::mLock failed to create lock!");
}

sbPropertyOperator::sbPropertyOperator(const nsAString& aOperator,
                                       const nsAString& aOperatorReadable)
: mLock(nsnull)
, mInitialized(PR_TRUE)
, mOperator(aOperator)
, mOperatorReadable(aOperatorReadable)
{
  mLock = PR_NewLock();
  NS_ASSERTION(mLock, "sbPropertyOperator::mLock failed to create lock!");
}

sbPropertyOperator::~sbPropertyOperator()
{
  if(mLock) {
    PR_DestroyLock(mLock);
  }
}

NS_IMETHODIMP sbPropertyOperator::GetOperator(nsAString & aOperator)
{
  nsAutoLock lock(mLock);
  aOperator = mOperator;

  return NS_OK;
}

NS_IMETHODIMP sbPropertyOperator::GetOperatorReadable(nsAString & aOperatorReadable)
{
  nsAutoLock lock(mLock);
  aOperatorReadable = mOperatorReadable;

  return NS_OK;
}

NS_IMETHODIMP sbPropertyOperator::Init(const nsAString & aOperator, const nsAString & aOperatorReadable)
{
  nsAutoLock lock(mLock);
  NS_ENSURE_TRUE(mInitialized == PR_FALSE, NS_ERROR_ALREADY_INITIALIZED);
  
  mOperator = aOperator;
  mOperatorReadable = aOperatorReadable;
  
  mInitialized = PR_TRUE;

  return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(sbPropertyInfo, sbIPropertyInfo)

sbPropertyInfo::sbPropertyInfo()
: mNullSort(sbIPropertyInfo::SORT_NULL_SMALL)
, mSortProfileLock(nsnull)
, mNameLock(nsnull)
, mTypeLock(nsnull)
, mDisplayNameLock(nsnull)
, mDisplayUsingSimpleTypeLock(nsnull)
, mDisplayUsingXBLWidgetLock(nsnull)
, mUnitsLock(nsnull)
, mOperatorsLock(nsnull)
{
  mSortProfileLock = PR_NewLock();
  NS_ASSERTION(mSortProfileLock, 
    "sbPropertyInfo::mSortProfileLock failed to create lock!");

  mNameLock = PR_NewLock();
  NS_ASSERTION(mNameLock, 
    "sbPropertyInfo::mNameLock failed to create lock!");

  mTypeLock = PR_NewLock();
  NS_ASSERTION(mTypeLock, 
    "sbPropertyInfo::mTypeLock failed to create lock!");

  mDisplayNameLock = PR_NewLock();
  NS_ASSERTION(mDisplayNameLock, 
    "sbPropertyInfo::mDisplayNameLock failed to create lock!");

  mDisplayUsingSimpleTypeLock = PR_NewLock();
  NS_ASSERTION(mDisplayUsingSimpleTypeLock, 
    "sbPropertyInfo::mDisplayUsingSimpleTypeLock failed to create lock!");

  mDisplayUsingXBLWidgetLock = PR_NewLock();
  NS_ASSERTION(mDisplayUsingXBLWidgetLock, 
    "sbPropertyInfo::mDisplayUsingXBLWidgetLock failed to create lock!");

  mUnitsLock = PR_NewLock();
  NS_ASSERTION(mUnitsLock, 
    "sbPropertyInfo::mUnitsLock failed to create lock!");

  mOperatorsLock = PR_NewLock();
  NS_ASSERTION(mOperatorsLock,
    "sbPropertyInfo::mOperatorsLock failed to create lock!");
}

sbPropertyInfo::~sbPropertyInfo()
{
  if(mSortProfileLock) {
    PR_DestroyLock(mSortProfileLock);
  }

  if(mNameLock) {
    PR_DestroyLock(mNameLock);
  }

  if(mTypeLock) {
    PR_DestroyLock(mTypeLock);
  }

  if(mDisplayNameLock) {
    PR_DestroyLock(mDisplayNameLock);
  }

  if(mDisplayUsingSimpleTypeLock) {
    PR_DestroyLock(mDisplayUsingSimpleTypeLock);
  }

  if(mDisplayUsingXBLWidgetLock) {
    PR_DestroyLock(mDisplayUsingXBLWidgetLock);
  }

  if(mUnitsLock) {
    PR_DestroyLock(mUnitsLock);
  }

  if(mOperatorsLock) {
    PR_DestroyLock(mOperatorsLock);
  }
}

NS_IMETHODIMP sbPropertyInfo::GetOPERATOR_EQUALS(nsAString & aOPERATOR_EQUALS)
{
  aOPERATOR_EQUALS = NS_LITERAL_STRING("=");
  return NS_OK;
}

NS_IMETHODIMP sbPropertyInfo::GetOPERATOR_NOTEQUALS(nsAString & aOPERATOR_NOTEQUALS)
{
  aOPERATOR_NOTEQUALS = NS_LITERAL_STRING("!=");  
  return NS_OK;
}

NS_IMETHODIMP sbPropertyInfo::GetOPERATOR_GREATER(nsAString & aOPERATOR_GREATER)
{
  aOPERATOR_GREATER = NS_LITERAL_STRING(">");  
  return NS_OK;
}

NS_IMETHODIMP sbPropertyInfo::GetOPERATOR_GREATEREQUAL(nsAString & aOPERATOR_GREATEREQUAL)
{
  aOPERATOR_GREATEREQUAL = NS_LITERAL_STRING(">=");  
  return NS_OK;
}

NS_IMETHODIMP sbPropertyInfo::GetOPERATOR_LESS(nsAString & aOPERATOR_LESS)
{
  aOPERATOR_LESS = NS_LITERAL_STRING("<");  
  return NS_OK;
}

NS_IMETHODIMP sbPropertyInfo::GetOPERATOR_LESSEQUAL(nsAString & aOPERATOR_LESSEQUAL)
{
  aOPERATOR_LESSEQUAL = NS_LITERAL_STRING("<=");  
  return NS_OK;
}

NS_IMETHODIMP sbPropertyInfo::GetOPERATOR_CONTAINS(nsAString & aOPERATOR_CONTAINS)
{
  aOPERATOR_CONTAINS = NS_LITERAL_STRING("%?%");  
  return NS_OK;
}

NS_IMETHODIMP sbPropertyInfo::GetOPERATOR_BEGINSWITH(nsAString & aOPERATOR_BEGINSWITH)
{
  aOPERATOR_BEGINSWITH = NS_LITERAL_STRING("?%");  
  return NS_OK;
}

NS_IMETHODIMP sbPropertyInfo::GetOPERATOR_ENDSWITH(nsAString & aOPERATOR_ENDSWITH)
{
  aOPERATOR_ENDSWITH = NS_LITERAL_STRING("%?");  
  return NS_OK;
}

NS_IMETHODIMP sbPropertyInfo::SetNullSort(PRUint32 aNullSort)
{
  mNullSort = aNullSort;
  return NS_OK; 
}
NS_IMETHODIMP sbPropertyInfo::GetNullSort(PRUint32 *aNullSort)
{
  NS_ENSURE_ARG_POINTER(aNullSort);
  *aNullSort = mNullSort;
  return NS_OK;
}

NS_IMETHODIMP sbPropertyInfo::SetSortProfile(sbIPropertyArray * aSortProfile)
{
  NS_ENSURE_ARG_POINTER(aSortProfile);
  
  nsAutoLock lock(mSortProfileLock);
  mSortProfile = aSortProfile;

  return NS_OK;
}
NS_IMETHODIMP sbPropertyInfo::GetSortProfile(sbIPropertyArray * *aSortProfile)
{
  NS_ENSURE_ARG_POINTER(aSortProfile);

  nsAutoLock lock(mSortProfileLock);
  *aSortProfile = mSortProfile;
  NS_ADDREF(*aSortProfile);

  return NS_OK;
}

NS_IMETHODIMP sbPropertyInfo::GetName(nsAString & aName)
{
  nsAutoLock lock(mNameLock);
  aName = mName;
  return NS_OK;
}
NS_IMETHODIMP sbPropertyInfo::SetName(const nsAString &aName)
{
  nsAutoLock lock(mNameLock);
  
  if(mName.IsEmpty()) {
    mName = aName;
    return NS_OK;
  }

  return NS_ERROR_ALREADY_INITIALIZED;
}

NS_IMETHODIMP sbPropertyInfo::GetType(nsAString & aType)
{
  nsAutoLock lock(mTypeLock);
  aType = mType;
  return NS_OK;
}
NS_IMETHODIMP sbPropertyInfo::SetType(const nsAString &aType)
{
  nsAutoLock lock(mTypeLock);

  if(mType.IsEmpty()) {
    mType = aType;
    return NS_OK;
  }

  return NS_ERROR_ALREADY_INITIALIZED;
}

NS_IMETHODIMP sbPropertyInfo::GetDisplayName(nsAString & aDisplayName)
{
  nsAutoLock lock(mDisplayNameLock);
  
  if(mDisplayName.IsEmpty()) {
    nsAutoLock lock(mNameLock);
    aDisplayName = mName;
  }
  else {
    aDisplayName = mDisplayName;
  }

  return NS_OK;
}
NS_IMETHODIMP sbPropertyInfo::SetDisplayName(const nsAString &aDisplayName)
{
  nsAutoLock lock(mDisplayNameLock);

  if(mDisplayName.IsEmpty()) {
    mDisplayName = aDisplayName;
    return NS_OK;
  }

  return NS_ERROR_ALREADY_INITIALIZED;
}

NS_IMETHODIMP sbPropertyInfo::GetDisplayUsingSimpleType(nsAString & aDisplayUsingSimpleType)
{
  nsAutoLock lock(mDisplayUsingSimpleTypeLock);
  aDisplayUsingSimpleType = mDisplayUsingSimpleType;
  return NS_OK;
}
NS_IMETHODIMP sbPropertyInfo::SetDisplayUsingSimpleType(const nsAString &aDisplayUsingSimpleType)
{
  nsAutoLock lock(mDisplayUsingSimpleTypeLock);

  if(mDisplayUsingSimpleType.IsEmpty()) {
    mDisplayUsingSimpleType = aDisplayUsingSimpleType;
    return NS_OK;
  }

  return NS_ERROR_ALREADY_INITIALIZED;
}

NS_IMETHODIMP sbPropertyInfo::GetDisplayUsingXBLWidget(nsIURI * *aDisplayUsingXBLWidget)
{
  NS_ENSURE_ARG_POINTER(aDisplayUsingXBLWidget);
  *aDisplayUsingXBLWidget = nsnull;
  
  nsAutoLock lock(mDisplayUsingXBLWidgetLock);
  if(mDisplayUsingXBLWidget) {
    
  }
    
  return NS_OK;
}
NS_IMETHODIMP sbPropertyInfo::SetDisplayUsingXBLWidget(nsIURI *aDisplayUsingXBLWidget)
{
  nsAutoLock lock(mDisplayUsingXBLWidgetLock);

  if(!mDisplayUsingXBLWidget) {
    PRBool isChromeURI = PR_FALSE;
    nsresult rv = aDisplayUsingXBLWidget->SchemeIs("chrome", &isChromeURI);
    NS_ENSURE_SUCCESS(rv, rv);

    if(isChromeURI) {
      mDisplayUsingXBLWidget = aDisplayUsingXBLWidget;
      return NS_OK;
    }
    
    return NS_ERROR_INVALID_ARG;
  }

  return NS_ERROR_ALREADY_INITIALIZED;
}

NS_IMETHODIMP sbPropertyInfo::GetUnits(nsAString & aUnits)
{
  nsAutoLock lock(mUnitsLock);
  aUnits = mUnits;
  return NS_OK;
}
NS_IMETHODIMP sbPropertyInfo::SetUnits(const nsAString & aUnits)
{
  nsAutoLock lock(mUnitsLock);

  if(mUnits.IsEmpty()) {
    mUnits = aUnits;
    return NS_OK;
  }

  return NS_ERROR_ALREADY_INITIALIZED;
}

NS_IMETHODIMP sbPropertyInfo::GetOperators(nsISimpleEnumerator * *aOperators)
{
  NS_ENSURE_ARG_POINTER(aOperators);

  nsAutoLock lock(mOperatorsLock);
  return NS_NewArrayEnumerator(aOperators, mOperators);
}
NS_IMETHODIMP sbPropertyInfo::SetOperators(nsISimpleEnumerator * aOperators)
{
  NS_ENSURE_ARG_POINTER(aOperators);

  nsAutoLock lock(mOperatorsLock);
  mOperators.Clear();

  PRBool hasMore = PR_FALSE;
  nsCOMPtr<nsISupports> object;

  while( NS_SUCCEEDED(aOperators->HasMoreElements(&hasMore)) && 
         hasMore  &&
         NS_SUCCEEDED(aOperators->GetNext(getter_AddRefs(object)))) {
    mOperators.AppendObject(object);
  }

  return NS_OK;
}

NS_IMETHODIMP sbPropertyInfo::Validate(const nsAString & aValue, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP sbPropertyInfo::Format(const nsAString & aValue, nsAString & _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP sbPropertyInfo::MakeSortable(const nsAString & aValue, nsAString & _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP sbPropertyInfo::GetDisplayPropertiesForValue(const nsAString& aValue, nsAString& _retval)
{
  _retval.Truncate();
  return NS_OK;
}
