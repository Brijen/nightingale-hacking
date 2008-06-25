/*
//
// BEGIN SONGBIRD GPL
//
// This file is part of the Songbird web player.
//
// Copyright(c) 2005-2008 POTI, Inc.
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

#include "sbPlaybackHistoryEntry.h"

#include <nsIClassInfoImpl.h>
#include <nsIProgrammingLanguage.h>
#include <nsIStringEnumerator.h>

#include <nsAutoLock.h>

NS_IMPL_THREADSAFE_ISUPPORTS1(sbPlaybackHistoryEntry, 
                              sbIPlaybackHistoryEntry)

sbPlaybackHistoryEntry::sbPlaybackHistoryEntry()
: mLock(nsnull)
, mTimestamp(0)
, mDuration(0)
{
  MOZ_COUNT_CTOR(sbPlaybackHistoryEntry);
}

sbPlaybackHistoryEntry::~sbPlaybackHistoryEntry()
{
  MOZ_COUNT_DTOR(sbPlaybackHistoryEntry);

  if(mLock) {
    nsAutoLock::DestroyLock(mLock);
  }
}

NS_IMETHODIMP 
sbPlaybackHistoryEntry::GetItem(sbIMediaItem * *aItem)
{
  NS_ENSURE_TRUE(mLock, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(aItem);

  nsAutoLock lock(mLock);
  NS_IF_ADDREF(*aItem = mItem);

  return NS_OK;
}

NS_IMETHODIMP 
sbPlaybackHistoryEntry::GetTimestamp(PRInt64 *aTimestamp)
{
  NS_ENSURE_TRUE(mLock, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(aTimestamp);

  nsAutoLock lock(mLock);
  *aTimestamp = mTimestamp;

  return NS_OK;
}

NS_IMETHODIMP 
sbPlaybackHistoryEntry::GetDuration(PRInt64 *aDuration)
{
  NS_ENSURE_TRUE(mLock, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(aDuration);

  nsAutoLock lock(mLock);
  *aDuration = mDuration;

  return NS_OK;
}

NS_IMETHODIMP 
sbPlaybackHistoryEntry::GetAnnotations(sbIPropertyArray * *aAnnotations)
{
  NS_ENSURE_TRUE(mLock, NS_ERROR_NOT_INITIALIZED);
  NS_ENSURE_ARG_POINTER(aAnnotations);

  nsAutoLock lock(mLock);
  NS_IF_ADDREF(*aAnnotations = mAnnotations);

  return NS_OK;
}

NS_IMETHODIMP 
sbPlaybackHistoryEntry::Init(sbIMediaItem *aItem, 
                             PRInt64 aTimestamp, 
                             PRInt64 aDuration, 
                             sbIPropertyArray *aAnnotations)
{
  NS_ENSURE_ARG_POINTER(aItem);
  NS_ENSURE_ARG_MIN(aTimestamp, 0);
  NS_ENSURE_ARG_MIN(aDuration, 0);

  mLock = nsAutoLock::NewLock("sbPlaybackHistoryEntry::mLock");
  NS_ENSURE_TRUE(mLock, NS_ERROR_OUT_OF_MEMORY);
  
  nsAutoLock lock(mLock);

  mItem = aItem;
  mTimestamp = aTimestamp;
  mDuration = aDuration;
  mAnnotations = aAnnotations;

  return NS_OK;
}
