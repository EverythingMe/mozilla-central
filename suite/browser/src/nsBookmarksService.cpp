/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8; c-file-style: "stroustrup" -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Robert John Churchill    <rjc@netscape.com>
 *   Chris Waterson           <waterson@netscape.com>
 *   Pierre Phaneuf           <pp@ludusdesign.com>
 */

#define NS_IMPL_IDS

/*
  The global bookmarks service.
 */

#include "nsCOMPtr.h"
#include "nsIFileSpec.h"
#include "nsCRT.h"
#include "nsFileStream.h"
#include "nsIBookmarksService.h"
#include "nsIComponentManager.h"
#include "nsIDOMWindow.h"
#include "nsIGenericFactory.h"
#include "nsIProfile.h"
#include "nsIRDFContainer.h"
#include "nsIRDFContainerUtils.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsIRDFService.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIScriptGlobalObject.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsRDFCID.h"
#include "nsSpecialSystemDirectory.h"
#include "nsString.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "prio.h"
#include "prlog.h"
#include "rdf.h"
#include "xp_core.h"
#include "prlong.h"
#include "prtime.h"
#include "nsEnumeratorUtils.h"
#include "nsEscape.h"
#include "nsITimer.h"
#include "nsIAtom.h"

//#include "nsISound.h"
//#include "nsICommonDialogs.h"
#include "nsINetSupportDialogService.h"
#include "nsIPrompt.h"
#include "nsAppShellCIDs.h"
#include "nsIAppShellService.h"
#include "nsIWebShell.h"
#include "nsWidgetsCID.h"
#include "nsIAppShell.h"

#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIIOService.h"
#include "nsIChannel.h"
#include "nsIHTTPChannel.h"
#include "nsHTTPEnums.h"

#include "nsIFileLocator.h"
#include "nsFileLocations.h"

#include "nsIStringBundle.h"

#include "nsIInputStream.h"
#include "nsIBufferInputStream.h"
#include "nsIStreamListener.h"
#include "nsIHTTPHeader.h"

#include "nsICharsetConverterManager.h"
#include "nsICharsetAlias.h"
#include "nsIPlatformCharset.h"
#include "nsIPref.h"

// Interfaces Needed
#include "nsIDocShell.h"
#include "nsIXULWindow.h"

#ifdef	DEBUG
#ifdef	XP_MAC
#include <Timer.h>
#endif
#endif

#define	BOOKMARK_TIMEOUT		15000		// fire every 15 seconds
//	#define	DEBUG_BOOKMARK_PING_OUTPUT	1

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_CID(kRDFInMemoryDataSourceCID,   NS_RDFINMEMORYDATASOURCE_CID);
static NS_DEFINE_CID(kRDFServiceCID,              NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFContainerCID,            NS_RDFCONTAINER_CID);
static NS_DEFINE_CID(kRDFContainerUtilsCID,       NS_RDFCONTAINERUTILS_CID);
static NS_DEFINE_CID(kFileLocatorCID,             NS_FILELOCATOR_CID); 
static NS_DEFINE_CID(kIOServiceCID,		  NS_IOSERVICE_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_CID(kNetSupportDialogCID,        NS_NETSUPPORTDIALOG_CID);
static NS_DEFINE_CID(kAppShellServiceCID,         NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_CID(kPrefCID,                    NS_PREF_CID);

static NS_DEFINE_CID(kStringBundleServiceCID,     NS_STRINGBUNDLESERVICE_CID);
static NS_DEFINE_CID(kPlatformCharsetCID,         NS_PLATFORMCHARSET_CID);

static const char kURINC_BookmarksRoot[]          = "NC:BookmarksRoot"; // XXX?
static const char kURINC_IEFavoritesRoot[]        = "NC:IEFavoritesRoot"; // XXX?
static const char kURINC_NewBookmarkFolder[]      = "NC:NewBookmarkFolder"; // XXX?
static const char kURINC_PersonalToolbarFolder[]  = "NC:PersonalToolbarFolder"; // XXX?
static const char kURINC_NewSearchFolder[]        = "NC:NewSearchFolder"; // XXX?
static const char kDefaultPersonalToolbarFolder[] = "Personal Toolbar Folder";
static const char kBookmarkCommand[]              = "http://home.netscape.com/NC-rdf#command?";

#define bookmark_properties "chrome://communicator/locale/bookmarks/bookmark.properties"

////////////////////////////////////////////////////////////////////////

PRInt32 gRefCnt;
nsIRDFService		*gRDF;
nsIRDFContainerUtils	*gRDFC;

nsIRDFResource		*kNC_Bookmark;
nsIRDFResource		*kNC_BookmarkSeparator;
nsIRDFResource		*kNC_BookmarkAddDate;
nsIRDFResource		*kNC_BookmarksRoot;
nsIRDFResource		*kNC_Description;
nsIRDFResource		*kNC_Folder;
nsIRDFResource		*kNC_FolderType;
nsIRDFResource		*kNC_IEFavorite;
nsIRDFResource		*kNC_IEFavoriteFolder;
nsIRDFResource		*kNC_IEFavoritesRoot;
nsIRDFResource		*kNC_Name;
nsIRDFResource		*kNC_NewBookmarkFolder;
nsIRDFResource		*kNC_NewSearchFolder;
nsIRDFResource		*kNC_PersonalToolbarFolder;
nsIRDFResource		*kNC_ShortcutURL;
nsIRDFResource		*kNC_URL;
nsIRDFResource		*kRDF_type;
nsIRDFResource		*kRDF_nextVal;
nsIRDFResource		*kWEB_LastModifiedDate;
nsIRDFResource		*kWEB_LastVisitDate;
nsIRDFResource		*kWEB_Schedule;
nsIRDFResource		*kWEB_Status;
nsIRDFResource		*kWEB_LastPingDate;
nsIRDFResource		*kWEB_LastPingETag;
nsIRDFResource		*kWEB_LastPingModDate;
nsIRDFResource		*kWEB_LastPingContentLen;

nsIRDFResource		*kNC_Parent;

nsIRDFResource		*kNC_BookmarkCommand_NewBookmark;
nsIRDFResource		*kNC_BookmarkCommand_NewFolder;
nsIRDFResource		*kNC_BookmarkCommand_NewSeparator;
nsIRDFResource		*kNC_BookmarkCommand_DeleteBookmark;
nsIRDFResource		*kNC_BookmarkCommand_DeleteBookmarkFolder;
nsIRDFResource		*kNC_BookmarkCommand_DeleteBookmarkSeparator;
nsIRDFResource		*kNC_BookmarkCommand_SetNewBookmarkFolder;
nsIRDFResource		*kNC_BookmarkCommand_SetPersonalToolbarFolder;
nsIRDFResource		*kNC_BookmarkCommand_SetNewSearchFolder;
nsIRDFResource		*kNC_BookmarkCommand_Import;
nsIRDFResource		*kNC_BookmarkCommand_Export;



static nsresult
bm_AddRefGlobals()
{
	if (gRefCnt++ == 0) {
		nsresult rv;
		rv = nsServiceManager::GetService(kRDFServiceCID,
						  NS_GET_IID(nsIRDFService),
						  (nsISupports**) &gRDF);

		NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
		if (NS_FAILED(rv)) return rv;

		rv = nsServiceManager::GetService(kRDFContainerUtilsCID,
						  NS_GET_IID(nsIRDFContainerUtils),
						  (nsISupports**) &gRDFC);

		NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF container utils");
		if (NS_FAILED(rv)) return rv;

		gRDF->GetResource(kURINC_BookmarksRoot,                   &kNC_BookmarksRoot);
		gRDF->GetResource(kURINC_IEFavoritesRoot,                 &kNC_IEFavoritesRoot);
		gRDF->GetResource(kURINC_NewBookmarkFolder,               &kNC_NewBookmarkFolder);
		gRDF->GetResource(kURINC_PersonalToolbarFolder,           &kNC_PersonalToolbarFolder);
		gRDF->GetResource(kURINC_NewSearchFolder,                 &kNC_NewSearchFolder);

		gRDF->GetResource(NC_NAMESPACE_URI "Bookmark",            &kNC_Bookmark);
		gRDF->GetResource(NC_NAMESPACE_URI "BookmarkSeparator",   &kNC_BookmarkSeparator);
		gRDF->GetResource(NC_NAMESPACE_URI "BookmarkAddDate",     &kNC_BookmarkAddDate);
		gRDF->GetResource(NC_NAMESPACE_URI "Description",         &kNC_Description);
		gRDF->GetResource(NC_NAMESPACE_URI "Folder",              &kNC_Folder);
		gRDF->GetResource(NC_NAMESPACE_URI "FolderType",          &kNC_FolderType);
		gRDF->GetResource(NC_NAMESPACE_URI "IEFavorite",          &kNC_IEFavorite);
		gRDF->GetResource(NC_NAMESPACE_URI "IEFavoriteFolder",    &kNC_IEFavoriteFolder);
		gRDF->GetResource(NC_NAMESPACE_URI "Name",                &kNC_Name);
		gRDF->GetResource(NC_NAMESPACE_URI "ShortcutURL",         &kNC_ShortcutURL);
		gRDF->GetResource(NC_NAMESPACE_URI "URL",                 &kNC_URL);
		gRDF->GetResource(RDF_NAMESPACE_URI "type",               &kRDF_type);
		gRDF->GetResource(RDF_NAMESPACE_URI "nextVal",            &kRDF_nextVal);

		gRDF->GetResource(WEB_NAMESPACE_URI "LastModifiedDate",   &kWEB_LastModifiedDate);
		gRDF->GetResource(WEB_NAMESPACE_URI "LastVisitDate",      &kWEB_LastVisitDate);

		gRDF->GetResource(WEB_NAMESPACE_URI "Schedule",           &kWEB_Schedule);
		gRDF->GetResource(WEB_NAMESPACE_URI "status",             &kWEB_Status);
		gRDF->GetResource(WEB_NAMESPACE_URI "LastPingDate",       &kWEB_LastPingDate);
		gRDF->GetResource(WEB_NAMESPACE_URI "LastPingETag",       &kWEB_LastPingETag);
		gRDF->GetResource(WEB_NAMESPACE_URI "LastPingModDate",    &kWEB_LastPingModDate);
		gRDF->GetResource(WEB_NAMESPACE_URI "LastPingContentLen", &kWEB_LastPingContentLen);

		gRDF->GetResource(NC_NAMESPACE_URI "parent",              &kNC_Parent);

		gRDF->GetResource(NC_NAMESPACE_URI "command?cmd=newbookmark",             &kNC_BookmarkCommand_NewBookmark);
		gRDF->GetResource(NC_NAMESPACE_URI "command?cmd=newfolder",               &kNC_BookmarkCommand_NewFolder);
		gRDF->GetResource(NC_NAMESPACE_URI "command?cmd=newseparator",            &kNC_BookmarkCommand_NewSeparator);
		gRDF->GetResource(NC_NAMESPACE_URI "command?cmd=deletebookmark",          &kNC_BookmarkCommand_DeleteBookmark);
		gRDF->GetResource(NC_NAMESPACE_URI "command?cmd=deletebookmarkfolder",    &kNC_BookmarkCommand_DeleteBookmarkFolder);
		gRDF->GetResource(NC_NAMESPACE_URI "command?cmd=deletebookmarkseparator", &kNC_BookmarkCommand_DeleteBookmarkSeparator);
		gRDF->GetResource(NC_NAMESPACE_URI "command?cmd=setnewbookmarkfolder",    &kNC_BookmarkCommand_SetNewBookmarkFolder);
		gRDF->GetResource(NC_NAMESPACE_URI "command?cmd=setpersonaltoolbarfolder",&kNC_BookmarkCommand_SetPersonalToolbarFolder);
		gRDF->GetResource(NC_NAMESPACE_URI "command?cmd=setnewsearchfolder",      &kNC_BookmarkCommand_SetNewSearchFolder);
		gRDF->GetResource(NC_NAMESPACE_URI "command?cmd=import",                  &kNC_BookmarkCommand_Import);
		gRDF->GetResource(NC_NAMESPACE_URI "command?cmd=export",                  &kNC_BookmarkCommand_Export);
	}
	return NS_OK;
}



static void
bm_ReleaseGlobals()
{
	if (--gRefCnt == 0)
	{
		if (gRDF)
		{
			nsServiceManager::ReleaseService(kRDFServiceCID, gRDF);
			gRDF = nsnull;
		}

		if (gRDFC)
		{
			nsServiceManager::ReleaseService(kRDFContainerUtilsCID, gRDFC);
			gRDFC = nsnull;
		}

		NS_IF_RELEASE(kNC_Bookmark);
		NS_IF_RELEASE(kNC_BookmarkSeparator);
		NS_IF_RELEASE(kNC_BookmarkAddDate);
		NS_IF_RELEASE(kNC_BookmarksRoot);
		NS_IF_RELEASE(kNC_Description);
		NS_IF_RELEASE(kNC_Folder);
		NS_IF_RELEASE(kNC_FolderType);
		NS_IF_RELEASE(kNC_IEFavorite);
		NS_IF_RELEASE(kNC_IEFavoriteFolder);
		NS_IF_RELEASE(kNC_IEFavoritesRoot);
		NS_IF_RELEASE(kNC_Name);
		NS_IF_RELEASE(kNC_NewBookmarkFolder);
		NS_IF_RELEASE(kNC_NewSearchFolder);
		NS_IF_RELEASE(kNC_PersonalToolbarFolder);
		NS_IF_RELEASE(kNC_ShortcutURL);
		NS_IF_RELEASE(kNC_URL);
		NS_IF_RELEASE(kRDF_type);
		NS_IF_RELEASE(kRDF_nextVal);
		NS_IF_RELEASE(kWEB_LastModifiedDate);
		NS_IF_RELEASE(kWEB_LastVisitDate);
		NS_IF_RELEASE(kWEB_Schedule);
		NS_IF_RELEASE(kWEB_Status);
		NS_IF_RELEASE(kWEB_LastPingDate);
		NS_IF_RELEASE(kWEB_LastPingETag);
		NS_IF_RELEASE(kWEB_LastPingModDate);
		NS_IF_RELEASE(kWEB_LastPingContentLen);
		NS_IF_RELEASE(kNC_Parent);

		NS_IF_RELEASE(kNC_BookmarkCommand_NewBookmark);
		NS_IF_RELEASE(kNC_BookmarkCommand_NewFolder);
		NS_IF_RELEASE(kNC_BookmarkCommand_NewSeparator);
		NS_IF_RELEASE(kNC_BookmarkCommand_DeleteBookmark);
		NS_IF_RELEASE(kNC_BookmarkCommand_DeleteBookmarkFolder);
		NS_IF_RELEASE(kNC_BookmarkCommand_DeleteBookmarkSeparator);
		NS_IF_RELEASE(kNC_BookmarkCommand_SetNewBookmarkFolder);
		NS_IF_RELEASE(kNC_BookmarkCommand_SetPersonalToolbarFolder);
		NS_IF_RELEASE(kNC_BookmarkCommand_SetNewSearchFolder);
		NS_IF_RELEASE(kNC_BookmarkCommand_Import);
		NS_IF_RELEASE(kNC_BookmarkCommand_Export);
	}
}



////////////////////////////////////////////////////////////////////////

class	nsBookmarksService;

/**
 * The bookmark parser knows how to read <tt>bookmarks.html</tt> and convert it
 * into an RDF graph.
 */
class BookmarkParser {
private:
	nsCOMPtr<nsIUnicodeDecoder>	mUnicodeDecoder;
	nsIRDFDataSource		*mDataSource;
	const char			*mIEFavoritesRoot;
	PRBool				mFoundIEFavoritesRoot;
	PRBool				mFoundPersonalToolbarFolder;
	char				*mContents;
	PRUint32			mContentsLen;
	PRInt32				mStartOffset;
	nsInputFileStream		*mInputStream;
	nsString			mPersonalToolbarName;

friend	class nsBookmarksService;

protected:
	nsresult AssertTime(nsIRDFResource* aSource,
			    nsIRDFResource* aLabel,
			    PRInt32 aTime);

	nsresult setFolderHint(nsIRDFResource *newSource, nsIRDFResource *objType);

	static nsresult CreateAnonymousResource(nsCOMPtr<nsIRDFResource>* aResult);

	nsresult Unescape(nsString &text);

	nsresult ParseMetaTag(const nsString &aLine, nsIUnicodeDecoder **decoder);

	nsresult ParseBookmark(const nsString &aLine,
			       const nsCOMPtr<nsIRDFContainer> &aContainer,
			       nsIRDFResource *nodeType, nsIRDFResource **bookmarkNode);

	nsresult ParseBookmarkHeader(const nsString &aLine,
				     const nsCOMPtr<nsIRDFContainer> &aContainer,
				     nsIRDFResource *nodeType);

	nsresult ParseBookmarkSeparator(const nsString &aLine,
					const nsCOMPtr<nsIRDFContainer> &aContainer);

	nsresult ParseHeaderBegin(const nsString &aLine,
				  const nsCOMPtr<nsIRDFContainer> &aContainer);

	nsresult ParseHeaderEnd(const nsString &aLine);

	nsresult ParseAttribute(const nsString &aLine, const char *aAttribute,
				PRInt32 aAttributeLen, nsString &aResult);

	PRInt32	getEOL(const char *whole, PRInt32 startOffset, PRInt32 totalLength);

public:
	BookmarkParser();
	~BookmarkParser();

	nsresult Init(nsFileSpec *fileSpec, nsIRDFDataSource *aDataSource, const nsString &defaultPersonalToolbarName);
	nsresult DecodeBuffer(nsString &line, char *buf, PRUint32 aLength);
	nsresult ProcessLine(nsIRDFContainer *aContainer, nsIRDFResource *nodeType,
			nsIRDFResource **bookmarkNode, nsString &line,
			nsString &description, PRBool &inDescription, PRBool &isActiveFlag);
	nsresult Parse(nsIRDFResource* aContainer, nsIRDFResource *nodeType);

	nsresult AddBookmark(nsCOMPtr<nsIRDFContainer> aContainer,
			     const char*      aURL,
			     const PRUnichar* aOptionalTitle,
			     PRInt32          aAddDate,
			     PRInt32          aLastVisitDate,
			     PRInt32          aLastModifiedDate,
			     const char*      aShortcutURL,
			     nsIRDFResource*  aNodeType,
			     nsIRDFResource** bookmarkNode);

	nsresult SetIEFavoritesRoot(const char *IEFavoritesRootURL)
	{
		mIEFavoritesRoot = IEFavoritesRootURL;
		return(NS_OK);
	}
	nsresult ParserFoundIEFavoritesRoot(PRBool *foundIEFavoritesRoot)
	{
		*foundIEFavoritesRoot = mFoundIEFavoritesRoot;
		return(NS_OK);
	}
	nsresult ParserFoundPersonalToolbarFolder(PRBool *foundPersonalToolbarFolder)
	{
		*foundPersonalToolbarFolder = mFoundPersonalToolbarFolder;
		return(NS_OK);
	}
};



BookmarkParser::BookmarkParser()
	: mContents(nsnull), mContentsLen(0L), mStartOffset(0L), mInputStream(nsnull)
{
	bm_AddRefGlobals();
}



nsresult
BookmarkParser::Init(nsFileSpec *fileSpec, nsIRDFDataSource *aDataSource, const nsString &defaultPersonalToolbarName)
{
	mDataSource = aDataSource;
	mIEFavoritesRoot = nsnull;
	mFoundIEFavoritesRoot = PR_FALSE;
	mFoundPersonalToolbarFolder = PR_FALSE;
	mPersonalToolbarName = defaultPersonalToolbarName;

	nsresult		rv;

	// determine default platform charset...
	NS_WITH_SERVICE(nsIPlatformCharset, platformCharset, kPlatformCharsetCID, &rv);
	if (NS_SUCCEEDED(rv) && (platformCharset))
	{
		nsAutoString	defaultCharset;
		if (NS_SUCCEEDED(rv = platformCharset->GetCharset(kPlatformCharsetSel_4xBookmarkFile, defaultCharset)))
		{
			// found the default platform charset, now try and get a decoder from it to Unicode
			NS_WITH_SERVICE(nsICharsetConverterManager, charsetConv, kCharsetConverterManagerCID, &rv);
			if (NS_SUCCEEDED(rv) && (charsetConv))
			{
				rv = charsetConv->GetUnicodeDecoder(&defaultCharset, getter_AddRefs(mUnicodeDecoder));
			}
		}
	}

	if (fileSpec)
	{
		mContentsLen = fileSpec->GetFileSize();
		if (mContentsLen > 0)
		{
			mContents = new char [mContentsLen + 1];
			if (mContents)
			{
				nsInputFileStream inputStream(*fileSpec);	// defaults to read only
			       	PRInt32 howMany = inputStream.read(mContents, mContentsLen);
			        if (PRUint32(howMany) == mContentsLen)
			        {
					mContents[mContentsLen] = '\0';
			        }
			        else
			        {
			        	delete [] mContents;
			        	mContents = nsnull;
			        }
			}
		}

		if (!mContents)
		{
			// we were unable to read in the entire bookmark file at once,
			// so let's try reading it in a bit at a time instead
			mInputStream = new nsInputFileStream(*fileSpec);
			if (mInputStream)
			{
				if (! mInputStream->is_open())
				{
					delete mInputStream;
					mInputStream = nsnull;
				}
			}
		}
	}
	return(NS_OK);
}



BookmarkParser::~BookmarkParser()
{
	if (mContents)
	{
		delete [] mContents;
		mContents = nsnull;
	}
	if (mInputStream)
	{
		delete mInputStream;
		mInputStream = nsnull;
	}
	bm_ReleaseGlobals();
}



static const char kHREFEquals[]   = "HREF=\"";
static const char kCloseAnchor[] = "</A>";

static const char kOpenHeading[]  = "<H";
static const char kCloseHeading[] = "</H";

static const char kSeparator[]  = "<HR>";

static const char kOpenUL[]     = "<UL>";
static const char kCloseUL[]    = "</UL>";

static const char kOpenMenu[]   = "<MENU>";
static const char kCloseMenu[]  = "</MENU>";

static const char kOpenDL[]     = "<DL>";
static const char kCloseDL[]    = "</DL>";

static const char kOpenDD[]     = "<DD>";

static const char kOpenMeta[]   = "<META ";

static const char kNewBookmarkFolderEquals[]      = "NEW_BOOKMARK_FOLDER=\"";
static const char kNewSearchFolderEquals[]        = "NEW_SEARCH_FOLDER=\"";
static const char kPersonalToolbarFolderEquals[]  = "PERSONAL_TOOLBAR_FOLDER=\"";

static const char kTargetEquals[]          = "TARGET=\"";
static const char kAddDateEquals[]         = "ADD_DATE=\"";
static const char kLastVisitEquals[]       = "LAST_VISIT=\"";
static const char kLastModifiedEquals[]    = "LAST_MODIFIED=\"";
static const char kShortcutURLEquals[]     = "SHORTCUTURL=\"";
static const char kScheduleEquals[]        = "SCHEDULE=\"";
static const char kLastPingEquals[]        = "LAST_PING=\"";
static const char kPingETagEquals[]        = "PING_ETAG=\"";
static const char kPingLastModEquals[]     = "PING_LAST_MODIFIED=\"";
static const char kPingContentLenEquals[]  = "PING_CONTENT_LEN=\"";
static const char kPingStatusEquals[]      = "PING_STATUS=\"";
static const char kIDEquals[]              = "ID=\"";
static const char kContentEquals[]         = "CONTENT=\"";
static const char kHTTPEquivEquals[]       = "HTTP-EQUIV=\"";
static const char kCharsetEquals[]         = "charset=";		// note: no quote



PRInt32
BookmarkParser::getEOL(const char *whole, PRInt32 startOffset, PRInt32 totalLength)
{
	PRInt32		eolOffset = -1;

	while (startOffset < totalLength)
	{
		char	c;
		c = whole[startOffset];
		if ((c == '\n') || (c == '\r') || (c == '\0'))
		{
			eolOffset = startOffset;
			break;
		}
		++startOffset;
	}
	return(eolOffset);
}



nsresult
BookmarkParser::DecodeBuffer(nsString &line, char *buf, PRUint32 aLength)
{
	if (mUnicodeDecoder)
	{
		nsresult	rv;
		char		*aBuffer = buf;
		PRInt32		unicharBufLen = 0;
		mUnicodeDecoder->GetMaxLength(aBuffer, aLength, &unicharBufLen);
		PRUnichar	*unichars = new PRUnichar [ unicharBufLen+1 ];
		do
		{
			PRInt32		srcLength = aLength;
			PRInt32		unicharLength = unicharBufLen;
			rv = mUnicodeDecoder->Convert(aBuffer, &srcLength, unichars, &unicharLength);
			unichars[unicharLength]=0;  //add this since the unicode converters can't be trusted to do so.

			// Move the nsParser.cpp 00 -> space hack to here so it won't break UCS2 file

			// Hack Start
			for(PRInt32 i=0;i<unicharLength-1; i++)
				if(0x0000 == unichars[i])	unichars[i] = 0x0020;
			// Hack End

			line.Append(unichars, unicharLength);
			// if we failed, we consume one byte by replace it with U+FFFD
			// and try conversion again.
			if(NS_FAILED(rv))
			{
				mUnicodeDecoder->Reset();
				line.Append( (PRUnichar)0xFFFD);
				if(((PRUint32) (srcLength + 1)) > (PRUint32)aLength)
					srcLength = aLength;
				else 
					srcLength++;
				aBuffer += srcLength;
				aLength -= srcLength;
			}
		} while (NS_FAILED(rv) && (aLength > 0));
		delete [] unichars;
		unichars = nsnull;
	}
	else
	{
		line.AppendWithConversion(buf, aLength);
	}
	return(NS_OK);
}



nsresult
BookmarkParser::ProcessLine(nsIRDFContainer *container, nsIRDFResource *nodeType,
			nsIRDFResource **bookmarkNode, nsString &line,
			nsString &description, PRBool &inDescription, PRBool &isActiveFlag)
{
	nsresult	rv = NS_OK;
	PRInt32		offset;

	if (*bookmarkNode)	*bookmarkNode = nsnull;

	if (inDescription == PR_TRUE)
	{
		offset = line.FindChar('<');
		if (offset < 0)
		{
			if (description.Length() > 0)
			{
				description.AppendWithConversion("\n");
			}
			description += line;
			return(NS_OK);
		}

		Unescape(description);

		if (*bookmarkNode)
		{
			nsCOMPtr<nsIRDFLiteral>	descLiteral;
			if (NS_SUCCEEDED(rv = gRDF->GetLiteral(description.GetUnicode(), getter_AddRefs(descLiteral))))
			{
				rv = mDataSource->Assert(*bookmarkNode, kNC_Description, descLiteral, PR_TRUE);
			}
		}

		inDescription = PR_FALSE;
		description.Truncate();
	}

	if ((offset = line.Find(kHREFEquals, PR_TRUE)) >= 0)
	{
		rv = ParseBookmark(line, container, nodeType, bookmarkNode);
	}
	else if ((offset = line.Find(kOpenMeta, PR_TRUE)) >= 0)
	{
		rv = ParseMetaTag(line, getter_AddRefs(mUnicodeDecoder));
	}
	else if ((offset = line.Find(kOpenHeading, PR_TRUE)) >= 0 &&
		 nsCRT::IsAsciiDigit(line.CharAt(offset + 2)))
	{
		// XXX Ignore <H1> so that bookmarks root _is_ <H1>
		if (line.CharAt(offset + 2) != PRUnichar('1'))
		{
			rv = ParseBookmarkHeader(line, container, nodeType);
		}
	}
	else if ((offset = line.Find(kSeparator, PR_TRUE)) >= 0)
	{
		rv = ParseBookmarkSeparator(line, container);
	}
	else if ((offset = line.Find(kCloseUL, PR_TRUE)) >= 0 ||
		 (offset = line.Find(kCloseMenu, PR_TRUE)) >= 0 ||
		 (offset = line.Find(kCloseDL, PR_TRUE)) >= 0)
	{
		isActiveFlag = PR_FALSE;
		return ParseHeaderEnd(line);
	}
	else if ((offset = line.Find(kOpenUL, PR_TRUE)) >= 0 ||
		 (offset = line.Find(kOpenMenu, PR_TRUE)) >= 0 ||
		 (offset = line.Find(kOpenDL, PR_TRUE)) >= 0)
	{
		rv = ParseHeaderBegin(line, container);
	}
	else if ((offset = line.Find(kOpenDD, PR_TRUE)) >= 0)
	{
		inDescription = PR_TRUE;
		line.Cut(0, offset+sizeof(kOpenDD)-1);
		description = line;
	}
	else
	{
		// XXX Discard the line?
	}
	return(rv);
}



nsresult
BookmarkParser::Parse(nsIRDFResource *aContainer, nsIRDFResource *nodeType)
{
	// tokenize the input stream.
	// XXX this needs to handle quotes, etc. it'd be nice to use the real parser for this...
	nsresult			rv;

	nsCOMPtr<nsIRDFContainer> container;
	rv = nsComponentManager::CreateInstance(kRDFContainerCID,
						nsnull,
						NS_GET_IID(nsIRDFContainer),
						getter_AddRefs(container));
	if (NS_FAILED(rv)) return rv;

	rv = container->Init(mDataSource, aContainer);
	if (NS_FAILED(rv)) return rv;

	nsCOMPtr<nsIRDFResource>	bookmarkNode = aContainer;
	nsAutoString			description, line;
	PRBool				isActiveFlag = PR_TRUE, inDescriptionFlag = PR_FALSE;

	if ((mContents) && (mContentsLen > 0))
	{
		// we were able to read the entire bookmark file into memory, so process it
		char				*linePtr;
		PRInt32				eol;

		while ((isActiveFlag == PR_TRUE) && (mStartOffset < (signed)mContentsLen))
		{
			linePtr = &mContents[mStartOffset];
			eol = getEOL(mContents, mStartOffset, mContentsLen);

			PRInt32	aLength;

			if ((eol >= mStartOffset) && (eol < (signed)mContentsLen))
			{
				// mContents[eol] = '\0';
				aLength = eol - mStartOffset;
				mStartOffset = eol + 1;
			}
			else
			{
				aLength = mContentsLen - mStartOffset;
				mStartOffset = mContentsLen + 1;
				isActiveFlag = PR_FALSE;
			}
			if (aLength < 1)	continue;

			line.Truncate();
			DecodeBuffer(line, linePtr, aLength);

			rv = ProcessLine(container, nodeType, getter_AddRefs(bookmarkNode),
				line, description, inDescriptionFlag, isActiveFlag);
			if (NS_FAILED(rv))	break;
		}
	}
	else if (mInputStream)
	{
		// we were unable to read in the entire bookmark file at once,
		// so let's try reading it in a bit at a time instead, and process it
		while (NS_SUCCEEDED(rv) && (isActiveFlag == PR_TRUE) &&
			(!mInputStream->eof()) && (!mInputStream->failed()))
		{
			line.Truncate();

			while (PR_TRUE)
			{
				char	buf[256];
				PRBool	untruncated = mInputStream->readline(buf, sizeof(buf));

				// in.readline() return PR_FALSE if there was buffer overflow,
				// or there was a catastrophe. Check to see if we're here
				// because of the latter...
				NS_ASSERTION (! mInputStream->failed(), "error reading file");
				if (mInputStream->failed())
				{
					rv = NS_ERROR_FAILURE;
					break;
				}

				PRUint32		aLength;
				if (untruncated)	aLength = strlen(buf);
				else			aLength = sizeof(buf);

				DecodeBuffer(line, buf, aLength);
				
				if (untruncated)	break;
			}
			if (NS_SUCCEEDED(rv))
			{
				rv = ProcessLine(container, nodeType, getter_AddRefs(bookmarkNode),
					line, description, inDescriptionFlag, isActiveFlag);
			}
		}
	}
	return(rv);
}



nsresult
BookmarkParser::Unescape(nsString &text)
{
	// convert some HTML-escaped (such as "&lt;") values back

	PRInt32		offset=0;

	while((offset = text.FindChar((PRUnichar('&')), PR_FALSE, offset)) >= 0)
	{
		// XXX get max of 6 chars; change the value below if
		// we ever start looking for longer HTML-escaped values
		nsAutoString	temp;
		text.Mid(temp, offset, 6);

		if (temp.CompareWithConversion("&lt;", PR_TRUE, 4) == 0)
		{
			text.Cut(offset, 4);
			text.Insert(PRUnichar('<'), offset);
		}
		else if (temp.CompareWithConversion("&gt;", PR_TRUE, 4) == 0)
		{
			text.Cut(offset, 4);
			text.Insert(PRUnichar('>'), offset);
		}
		else if (temp.CompareWithConversion("&amp;", PR_TRUE, 5) == 0)
		{
			text.Cut(offset, 5);
			text.Insert(PRUnichar('&'), offset);
		}
		else if (temp.CompareWithConversion("&quot;", PR_TRUE, 6) == 0)
		{
			text.Cut(offset, 6);
			text.Insert(PRUnichar('\"'), offset);
		}

		++offset;
	}
	return(NS_OK);
}



nsresult
BookmarkParser::CreateAnonymousResource(nsCOMPtr<nsIRDFResource>* aResult)
{
	static PRInt32 gNext = 0;
	if (! gNext) {
		LL_L2I(gNext, PR_Now());
	}
	nsAutoString uri; uri.AssignWithConversion(kURINC_BookmarksRoot);
	uri.AppendWithConversion("#$");
	uri.AppendInt(++gNext, 16);

	return gRDF->GetUnicodeResource(uri.GetUnicode(), getter_AddRefs(*aResult));
}



nsresult
BookmarkParser::ParseMetaTag(const nsString &aLine, nsIUnicodeDecoder **decoder)
{
	nsresult	rv = NS_OK;

	*decoder = nsnull;

	// get the HTTP-EQUIV attribute
	PRInt32 start = aLine.Find(kHTTPEquivEquals, PR_TRUE);
	NS_ASSERTION(start >= 0, "no 'HTTP-EQUIV=\"' string: how'd we get here?");
	if (start < 0)	return(NS_ERROR_UNEXPECTED);
	// Skip past the first double-quote
	start += (sizeof(kHTTPEquivEquals) - 1);
	// ...and find the next so we can chop the HTTP-EQUIV attribute
	PRInt32 end = aLine.FindChar(PRUnichar('"'), PR_FALSE, start);
	nsAutoString	httpEquiv;
	aLine.Mid(httpEquiv, start, end - start);

	// if HTTP-EQUIV isn't "Content-Type", just ignore the META tag
	if (!httpEquiv.EqualsIgnoreCase("Content-Type"))
		return(NS_OK);

	// get the CONTENT attribute
	start = aLine.Find(kContentEquals, PR_TRUE);
	NS_ASSERTION(start >= 0, "no 'CONTENT=\"' string: how'd we get here?");
	if (start < 0)	return(NS_ERROR_UNEXPECTED);
	// Skip past the first double-quote
	start += (sizeof(kContentEquals) - 1);
	// ...and find the next so we can chop the CONTENT attribute
	end = aLine.FindChar(PRUnichar('"'), PR_FALSE, start);
	nsAutoString	content;
	aLine.Mid(content, start, end - start);

	// look for the charset value
	start = content.Find(kCharsetEquals, PR_TRUE);
	NS_ASSERTION(start >= 0, "no 'charset=' string: how'd we get here?");
	if (start < 0)	return(NS_ERROR_UNEXPECTED);
	start += (sizeof(kCharsetEquals)-1);
	nsAutoString	charset;
	content.Mid(charset, start, content.Length() - start);
	if (charset.Length() < 1)	return(NS_ERROR_UNEXPECTED);

	NS_WITH_SERVICE(nsICharsetAlias, calias, kCharsetAliasCID, &rv);
	if (NS_SUCCEEDED(rv) && (calias))
	{
		nsAutoString	charsetName;
		if (NS_SUCCEEDED(rv = calias->GetPreferred(charset, charsetName)))
		{
			if (charsetName.Length() > 0)
			{
				charset = charsetName;
			}
		}
	}

	// found a charset, now try and get a decoder from it to Unicode
	nsICharsetConverterManager	*charsetConv = nsnull;
	rv = nsServiceManager::GetService(kCharsetConverterManagerCID, 
			NS_GET_IID(nsICharsetConverterManager), 
			(nsISupports**)&charsetConv);
	if (NS_SUCCEEDED(rv) && (charsetConv))
	{
		rv = charsetConv->GetUnicodeDecoder(&charset, decoder);
		NS_RELEASE(charsetConv);
	}
	return(rv);
}



nsresult
BookmarkParser::ParseBookmark(const nsString &aLine, const nsCOMPtr<nsIRDFContainer> &aContainer,
				nsIRDFResource *nodeType, nsIRDFResource **bookmarkNode)
{
	NS_PRECONDITION(aContainer != nsnull, "null ptr");
	if (! aContainer)
		return NS_ERROR_NULL_POINTER;

	PRInt32 start = aLine.Find(kHREFEquals, PR_TRUE);
	NS_ASSERTION(start >= 0, "no 'HREF=\"' string: how'd we get here?");
	if (start < 0)
		return NS_ERROR_UNEXPECTED;

	// 1. Crop out the URL

	// Skip past the first double-quote
	start += (sizeof(kHREFEquals) - 1);

	// ...and find the next so we can chop the URL.
	PRInt32 end = aLine.FindChar(PRUnichar('"'), PR_FALSE,start);
	NS_ASSERTION(end >= 0, "unterminated string");
	if (end < 0)
		return NS_ERROR_UNEXPECTED;

	nsAutoString url;
	aLine.Mid(url, start, end - start);

	{
		// Now do properly replace %22's; this is particularly important for javascript: URLs
		static const char kEscape22[] = "%22";
		PRInt32 offset;
		while ((offset = url.Find(kEscape22)) >= 0)
		{
			url.SetCharAt('\"',offset);
			url.Cut(offset + 1, sizeof(kEscape22) - 2);
		}
	}

	// XXX At this point, the URL may be relative. 4.5 called into
	// netlib to make an absolute URL, and there was some magic
	// "relative_URL" parameter that got sent down as well. We punt on
	// that stuff.

	// 2. Parse the name

	start = aLine.FindChar(PRUnichar('>'), PR_FALSE,end + 1); // 'end' still points to the end of the URL
	if (start < 0)
	{
		NS_WARNING("open anchor tag not terminated");
		return NS_ERROR_UNEXPECTED;
	}
	
	nsAutoString name;
	aLine.Right(name, aLine.Length() - (start + 1));
	end = name.Find(kCloseAnchor, PR_TRUE);
	if (end < 0)
	{
		NS_WARNING("anchor tag not terminated");
		return NS_ERROR_UNEXPECTED;
	}
	name.Truncate(end);
	Unescape(name);

	// 3. Parse the target
	nsAutoString	target;

	start = aLine.Find(kTargetEquals, PR_TRUE);
	if (start >= 0)
	{
		start += (sizeof(kTargetEquals) - 1);
		end = aLine.FindChar(PRUnichar('"'), PR_FALSE,start);
		aLine.Mid(target, start, end - start);
	}


	// 4. Parse the addition date
	PRInt32 addDate = 0;
	{
		nsAutoString s;
		ParseAttribute(aLine, kAddDateEquals, sizeof(kAddDateEquals) - 1, s);
		if (s.Length() > 0) {
			PRInt32 err;
			addDate = s.ToInteger(&err); // ignored.
		}
	}

	// 5. Parse the last visit date
	PRInt32 lastVisitDate = 0;
	{
		nsAutoString s;
		ParseAttribute(aLine, kLastVisitEquals, sizeof(kLastVisitEquals) - 1, s);
		if (s.Length() > 0) {
			PRInt32 err;
			lastVisitDate = s.ToInteger(&err); // ignored.
		}
	}

	// 6. Parse the last modified date
	PRInt32 lastModifiedDate = 0;
	{
		nsAutoString s;
		ParseAttribute(aLine, kLastModifiedEquals, sizeof(kLastModifiedEquals) - 1, s);
		if (s.Length() > 0) {
			PRInt32 err;
			lastModifiedDate = s.ToInteger(&err); // ignored.
		}
	}

	// 7. Parse the shortcut URL (and always lowercase them before storing internally)
	nsAutoString	shortcut;
	ParseAttribute(aLine, kShortcutURLEquals, sizeof(kShortcutURLEquals) -1, shortcut);
	shortcut.ToLowerCase();

	// 8. Parse the schedule
	nsAutoString	schedule;
	ParseAttribute(aLine, kScheduleEquals, sizeof(kScheduleEquals) -1, schedule);

	// 9. Parse the last ping date
	PRInt32 lastPingDate = 0;
	{
		nsAutoString s;
		ParseAttribute(aLine, kLastPingEquals, sizeof(kLastPingEquals) - 1, s);
		if (s.Length() > 0) {
			PRInt32 err;
			lastPingDate = s.ToInteger(&err); // ignored.
		}
	}

	// 10. Parse the ping ETag
	nsAutoString	pingETag;
	ParseAttribute(aLine, kPingETagEquals, sizeof(kPingETagEquals) -1, pingETag);

	// 11. Parse the ping LastMod date
	nsAutoString	pingLastMod;
	ParseAttribute(aLine, kPingLastModEquals, sizeof(kPingLastModEquals) -1, pingLastMod);

	// 12. Parse the Ping Content Length
	nsAutoString	pingContentLength;
	ParseAttribute(aLine, kPingContentLenEquals, sizeof(kPingContentLenEquals) -1, pingContentLength);

	// 13. Parse the Ping Status
	nsAutoString	pingStatus;
	ParseAttribute(aLine, kPingStatusEquals, sizeof(kPingStatusEquals) -1, pingStatus);

	// Dunno. 4.5 did it, so will we.
	if (!lastModifiedDate)
		lastModifiedDate = lastVisitDate;

	// There was some other cruft here to deal with aliases, but we ignore them thanks to RDF

	nsresult rv = NS_ERROR_OUT_OF_MEMORY; // in case ToNewCString() fails

	char *cURL = url.ToNewCString();
	if (cURL)
	{
		char *cShortcutURL = shortcut.ToNewCString();	// Note: can be null

		rv = AddBookmark(aContainer, cURL, name.GetUnicode(), addDate, lastVisitDate,
				lastModifiedDate, cShortcutURL, nodeType, bookmarkNode);

		if (NS_SUCCEEDED(rv))
		{
			// save schedule
			if (schedule.Length() > 0)
			{
				nsCOMPtr<nsIRDFLiteral>	scheduleLiteral;
				if (NS_SUCCEEDED(rv = gRDF->GetLiteral(schedule.GetUnicode(),
								    getter_AddRefs(scheduleLiteral))))
				{
					rv = mDataSource->Assert(*bookmarkNode, kWEB_Schedule, scheduleLiteral, PR_TRUE);
					if (rv != NS_RDF_ASSERTION_ACCEPTED)
					{
						NS_ERROR("unable to set bookmark schedule");
					}
				}
				else
				{
					NS_ERROR("unable to get literal for bookmark schedule");
				}
			}

			// last ping date
			AssertTime(*bookmarkNode, kWEB_LastPingDate, lastPingDate);
				
			// save ping ETag
			if (pingETag.Length() > 0)
			{
				PRInt32		offset;

				// Note: don't allow quotes in etag
				while ((offset = pingETag.FindChar('\"')) >= 0)
				{
					pingETag.Cut(offset, 1);
				}

				nsCOMPtr<nsIRDFLiteral>	pingLiteral;
				if (NS_SUCCEEDED(rv = gRDF->GetLiteral(pingETag.GetUnicode(),
								    getter_AddRefs(pingLiteral))))
				{
					rv = mDataSource->Assert(*bookmarkNode, kWEB_LastPingETag, pingLiteral, PR_TRUE);
					if (rv != NS_RDF_ASSERTION_ACCEPTED)
					{
						NS_ERROR("unable to set ping etag");
					}
				}
				else
				{
					NS_ERROR("unable to get literal for ping etag");
				}
			}

			// save ping Last Mod date
			if (pingLastMod.Length() > 0)
			{
				nsCOMPtr<nsIRDFLiteral>	pingLastModLiteral;
				if (NS_SUCCEEDED(rv = gRDF->GetLiteral(pingLastMod.GetUnicode(),
								    getter_AddRefs(pingLastModLiteral))))
				{
					rv = mDataSource->Assert(*bookmarkNode, kWEB_LastPingModDate, pingLastModLiteral, PR_TRUE);
					if (rv != NS_RDF_ASSERTION_ACCEPTED)
					{
						NS_ERROR("unable to set ping last mod");
					}
				}
				else
				{
					NS_ERROR("unable to get literal for ping last mod");
				}
			}

			// save ping Content Length date
			if (pingContentLength.Length() > 0)
			{
				nsCOMPtr<nsIRDFLiteral>	pingContentLengthLiteral;
				if (NS_SUCCEEDED(rv = gRDF->GetLiteral(pingContentLength.GetUnicode(),
								    getter_AddRefs(pingContentLengthLiteral))))
				{
					rv = mDataSource->Assert(*bookmarkNode, kWEB_LastPingContentLen, pingContentLengthLiteral, PR_TRUE);
					if (rv != NS_RDF_ASSERTION_ACCEPTED)
					{
						NS_ERROR("unable to set ping content length");
					}
				}
				else
				{
					NS_ERROR("unable to get literal for ping content length");
				}
			}

			// save ping status
			if (pingStatus.Length() > 0)
			{
				nsCOMPtr<nsIRDFLiteral>	pingStatusLiteral;
				if (NS_SUCCEEDED(rv = gRDF->GetLiteral(pingStatus.GetUnicode(),
								    getter_AddRefs(pingStatusLiteral))))
				{
					rv = mDataSource->Assert(*bookmarkNode, kWEB_Status, pingStatusLiteral, PR_TRUE);
					if (rv != NS_RDF_ASSERTION_ACCEPTED)
					{
						NS_ERROR("unable to set ping status");
					}
				}
				else
				{
					NS_ERROR("unable to get literal for ping status");
				}
			}
		}
		if (cShortcutURL)
		{
			nsCRT::free(cShortcutURL);
		}
		nsCRT::free(cURL);
	}
	return(rv);
}



    // Now create the bookmark
nsresult
BookmarkParser::AddBookmark(nsCOMPtr<nsIRDFContainer> aContainer,
                            const char*      aURL,
                            const PRUnichar* aOptionalTitle,
                            PRInt32          aAddDate,
                            PRInt32          aLastVisitDate,
                            PRInt32          aLastModifiedDate,
                            const char*      aShortcutURL,
                            nsIRDFResource*  aNodeType,
                            nsIRDFResource** bookmarkNode)
{
	nsresult	rv;
	nsAutoString	fullURL; fullURL.AssignWithConversion(aURL);

	// hack fix for bug # 21175:
	// if we don't have a protocol scheme, add "http://" as a default scheme
	if (fullURL.FindChar(PRUnichar(':')) < 0)
	{
		fullURL.InsertWithConversion("http://", 0);
	}

	nsCOMPtr<nsIRDFResource> bookmark;
  nsCAutoString fullurlC;
  fullurlC.AssignWithConversion(fullURL);
	if (NS_FAILED(rv = gRDF->GetResource(fullurlC, getter_AddRefs(bookmark) )))
	{
		NS_ERROR("unable to get bookmark resource");
		return(rv);
	}

	if (bookmarkNode)
	{
		*bookmarkNode = bookmark;
		NS_ADDREF(*bookmarkNode);
	}

	PRBool		isIEFavoriteRoot = PR_FALSE;

	if (nsnull != mIEFavoritesRoot)
	{
		if (!PL_strcmp(aURL, mIEFavoritesRoot))
		{
			mFoundIEFavoritesRoot = PR_TRUE;
			isIEFavoriteRoot = PR_TRUE;
		}
	}

	if (isIEFavoriteRoot == PR_TRUE)
	{
		rv = mDataSource->Assert(bookmark, kRDF_type, kNC_IEFavoriteFolder, PR_TRUE);
	}
	else
	{
		rv = mDataSource->Assert(bookmark, kRDF_type, aNodeType, PR_TRUE);
	}

	if (rv != NS_RDF_ASSERTION_ACCEPTED)
	{
		NS_ERROR("unable to add bookmark to data source");
		return(rv);
	}

	if ((nsnull != aOptionalTitle) && (*aOptionalTitle != PRUnichar('\0')))
	{
		nsCOMPtr<nsIRDFLiteral> literal;
		if (NS_FAILED(rv = gRDF->GetLiteral(aOptionalTitle, getter_AddRefs(literal))))
		{
			NS_ERROR("unable to create literal for bookmark name");
		}
		if (NS_SUCCEEDED(rv))
		{
			rv = mDataSource->Assert(bookmark, kNC_Name, literal, PR_TRUE);
			if (rv != NS_RDF_ASSERTION_ACCEPTED)
			{
				NS_ERROR("unable to set bookmark name");
			}
		}
	}

	AssertTime(bookmark, kNC_BookmarkAddDate, aAddDate);
	AssertTime(bookmark, kWEB_LastVisitDate, aLastVisitDate);
	AssertTime(bookmark, kWEB_LastModifiedDate, aLastModifiedDate);

	if ((nsnull != aShortcutURL) && (*aShortcutURL != '\0'))
	{
		nsCOMPtr<nsIRDFLiteral> shortcutLiteral;
		if (NS_FAILED(rv = gRDF->GetLiteral(NS_ConvertASCIItoUCS2(aShortcutURL).GetUnicode(),
						    getter_AddRefs(shortcutLiteral))))
		{
			NS_ERROR("unable to get literal for bookmark shortcut URL");
		}
		if (NS_SUCCEEDED(rv) && (rv != NS_RDF_NO_VALUE))
		{
			rv = mDataSource->Assert(bookmark,
						 kNC_ShortcutURL,
						 shortcutLiteral,
						 PR_TRUE);

			if (rv != NS_RDF_ASSERTION_ACCEPTED)
			{
				NS_ERROR("unable to set bookmark shortcut URL");
			}
		}
	}

	// The last thing we do is add the bookmark to the container. This ensures the minimal amount of reflow.
	rv = aContainer->AppendElement(bookmark);
	NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add bookmark to container");
	return(rv);
}



nsresult
BookmarkParser::ParseBookmarkHeader(const nsString &aLine,
				    const nsCOMPtr<nsIRDFContainer> &aContainer,
				    nsIRDFResource *nodeType)
{
	// Snip out the header
	PRInt32 start = aLine.Find(kOpenHeading, PR_TRUE);
	NS_ASSERTION(start >= 0, "couldn't find '<H'; why am I here?");
	if (start < 0)
		return NS_ERROR_UNEXPECTED;

	start += (sizeof(kOpenHeading) - 1);
	start = aLine.FindChar(PRUnichar('>'), PR_FALSE,start); // skip to the end of the '<Hn>' tag

	if (start < 0)
	{
		NS_WARNING("couldn't find end of header tag");
		return NS_OK;
	}

	nsAutoString name;
	aLine.Right(name, aLine.Length() - (start + 1));

	PRInt32 end = name.Find(kCloseHeading, PR_TRUE);
	if (end < 0)
		NS_WARNING("No '</H' found to close the heading");

	if (end >= 0)
		name.Truncate(end);

	// Find the add date
	PRInt32 addDate = 0;

	nsAutoString s;
	ParseAttribute(aLine, kAddDateEquals, sizeof(kAddDateEquals) - 1, s);
	if (s.Length() > 0)
	{
		PRInt32 err;
		addDate = s.ToInteger(&err); // ignored
	}

	// Find the lastmod date
	PRInt32 lastmodDate = 0;

	ParseAttribute(aLine, kLastModifiedEquals, sizeof(kLastModifiedEquals) - 1, s);
	if (s.Length() > 0)
	{
		PRInt32 err;
		lastmodDate = s.ToInteger(&err); // ignored
	}

	nsAutoString id;
	ParseAttribute(aLine, kIDEquals, sizeof(kIDEquals) - 1, id);

	// Parse various magical type hints
	nsAutoString	newBookmarkFolderHint;
	ParseAttribute(aLine, kNewBookmarkFolderEquals, sizeof(kNewBookmarkFolderEquals) - 1,
		newBookmarkFolderHint);

	nsAutoString	newSearchFolderHint;
	ParseAttribute(aLine, kNewSearchFolderEquals, sizeof(kNewSearchFolderEquals) - 1,
		newSearchFolderHint);

	nsAutoString	personalToolbarFolderHint;
	ParseAttribute(aLine, kPersonalToolbarFolderEquals, sizeof(kPersonalToolbarFolderEquals) - 1,
		personalToolbarFolderHint);

	// Make the necessary assertions
	nsresult rv;
	nsCOMPtr<nsIRDFResource> folder;

	if (id.EqualsIgnoreCase(kURINC_PersonalToolbarFolder))
	{
		mFoundPersonalToolbarFolder = PR_TRUE;
		personalToolbarFolderHint.AssignWithConversion("true");
		folder = dont_QueryInterface( kNC_PersonalToolbarFolder );
	}
	else if (id.Length() > 0)
	{
		// Use the ID attribute, if one is set.
		rv = gRDF->GetUnicodeResource(id.GetUnicode(), getter_AddRefs(folder));
		NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create resource for folder");
		if (NS_FAILED(rv)) return rv;
	}
	else
	{
		// We've never seen this folder before. Assign it an anonymous ID
		rv = CreateAnonymousResource(&folder);
		NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create anonymous resource for folder");
		if (NS_FAILED(rv)) return rv;
	}

	nsCOMPtr<nsIRDFLiteral> literal;
	rv = gRDF->GetLiteral(name.GetUnicode(), getter_AddRefs(literal));
	NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create literal for folder name");
	if (NS_FAILED(rv)) return rv;

	rv = mDataSource->Assert(folder, kNC_Name, literal, PR_TRUE);
	if (rv != NS_RDF_ASSERTION_ACCEPTED)
	{
		NS_ERROR("unable to set folder name");
		return rv;
	}

	rv = gRDFC->MakeSeq(mDataSource, folder, nsnull);
	NS_ASSERTION(NS_SUCCEEDED(rv), "unable to make new folder as sequence");
	if (NS_FAILED(rv)) return rv;

	// set hints
	if (newBookmarkFolderHint.EqualsIgnoreCase("true"))
	{
		rv = setFolderHint(folder, kNC_NewBookmarkFolder);
	}
	if (newSearchFolderHint.EqualsIgnoreCase("true"))
	{
		rv = setFolderHint(folder, kNC_NewSearchFolder);
	}
	if (personalToolbarFolderHint.EqualsIgnoreCase("true"))
	{
		rv = setFolderHint(folder, kNC_PersonalToolbarFolder);
		mFoundPersonalToolbarFolder = PR_TRUE;
	}

	PRBool		isIEFavoriteRoot = PR_FALSE;
	if (nsnull != mIEFavoritesRoot)
	{
		if (id.EqualsWithConversion(mIEFavoritesRoot))
		{
			isIEFavoriteRoot = PR_TRUE;
		}
	}

	if ((isIEFavoriteRoot == PR_TRUE) || (nodeType == kNC_IEFavorite))
	{
		rv = mDataSource->Assert(folder, kRDF_type, kNC_IEFavoriteFolder, PR_TRUE);
	}
	else
	{
		rv = mDataSource->Assert(folder, kRDF_type, kNC_Folder, PR_TRUE);
	}
	if (rv != NS_RDF_ASSERTION_ACCEPTED)
	{
		NS_ERROR("unable to mark new folder as folder");
		return rv;
	}

	if (NS_FAILED(rv = AssertTime(folder, kNC_BookmarkAddDate, addDate)))
	{
		NS_ERROR("unable to mark add date");
		return rv;
	}
	if (NS_FAILED(rv = AssertTime(folder, kWEB_LastModifiedDate, lastmodDate)))
	{
		NS_ERROR("unable to mark lastmod date");
		return rv;
	}

	// And now recursively parse the rest of the file...

	if (NS_FAILED(rv = Parse(folder, nodeType)))
	{
		NS_WARNING("recursive parse of bookmarks file failed");
		return rv;
	}

	// rjc: always do this last
	rv = aContainer->AppendElement(folder);
	NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add folder to container");
	if (NS_FAILED(rv)) return rv;

	return NS_OK;
}



nsresult
BookmarkParser::ParseBookmarkSeparator(const nsString &aLine, const nsCOMPtr<nsIRDFContainer> &aContainer)
{
	nsresult			rv;
	nsCOMPtr<nsIRDFResource>	separator;

	if (NS_SUCCEEDED(rv = CreateAnonymousResource(&separator)))
	{
		nsAutoString		defaultSeparatorName; defaultSeparatorName.AssignWithConversion("-----");
		nsCOMPtr<nsIRDFLiteral> nameLiteral;
		if (NS_SUCCEEDED(rv = gRDF->GetLiteral(defaultSeparatorName.GetUnicode(), getter_AddRefs(nameLiteral))))
		{
			if (NS_SUCCEEDED(rv = mDataSource->Assert(separator, kNC_Name, nameLiteral, PR_TRUE)))
			{
			}
		}
		if (NS_SUCCEEDED(rv = mDataSource->Assert(separator, kRDF_type, kNC_BookmarkSeparator, PR_TRUE)))
		{
			rv = aContainer->AppendElement(separator);
			if (NS_FAILED(rv)) return rv;
		}
	}
	return(rv);
}



nsresult
BookmarkParser::ParseHeaderBegin(const nsString &aLine, const nsCOMPtr<nsIRDFContainer> &aContainer)
{
	return(NS_OK);
}



nsresult
BookmarkParser::ParseHeaderEnd(const nsString &aLine)
{
	return(NS_OK);
}



nsresult
BookmarkParser::ParseAttribute(const nsString &aLine,
                               const char *aAttributeName,
                               PRInt32 aAttributeLen,
                               nsString &aResult)
{
	aResult.Truncate();

	PRInt32 start = aLine.Find(aAttributeName, PR_TRUE);
	if (start < 0)
		return NS_OK;

	start += aAttributeLen;
	PRInt32 end = aLine.FindChar(PRUnichar('"'), PR_FALSE,start);
	aLine.Mid(aResult, start, end - start);

	return NS_OK;
}



nsresult
BookmarkParser::AssertTime(nsIRDFResource* aSource,
                           nsIRDFResource* aLabel,
                           PRInt32 aTime)
{
	nsresult	rv = NS_OK;

	if (aTime != 0)
	{
		// Convert to a date literal
		PRInt64		dateVal, temp, million;

		LL_I2L(temp, aTime);
		LL_I2L(million, PR_USEC_PER_SEC);
		LL_MUL(dateVal, temp, million);		// convert from seconds to microseconds (PRTime)

		nsCOMPtr<nsIRDFDate>	dateLiteral;
		if (NS_FAILED(rv = gRDF->GetDateLiteral(dateVal, getter_AddRefs(dateLiteral))))
		{
			NS_ERROR("unable to get date literal for time");
			return(rv);
		}
		nsCOMPtr<nsIRDFNode>	currentNode;
		if (NS_SUCCEEDED(rv = mDataSource->GetTarget(aSource, aLabel, PR_TRUE,
			getter_AddRefs(currentNode))) && (rv != NS_RDF_NO_VALUE))
		{
			rv = mDataSource->Change(aSource, aLabel, currentNode, dateLiteral);
		}
		else
		{
			rv = mDataSource->Assert(aSource, aLabel, dateLiteral, PR_TRUE);
		}
		NS_ASSERTION(rv == NS_RDF_ASSERTION_ACCEPTED, "unable to assert new time");
	}
	return(rv);
}



nsresult
BookmarkParser::setFolderHint(nsIRDFResource *newSource, nsIRDFResource *objType)
{
	nsresult			rv;
	nsCOMPtr<nsISimpleEnumerator>	srcList;
	if (NS_FAILED(rv = mDataSource->GetSources(kNC_FolderType, objType, PR_TRUE, getter_AddRefs(srcList))))
		return(rv);

	PRBool	hasMoreSrcs = PR_TRUE;
	while(NS_SUCCEEDED(rv = srcList->HasMoreElements(&hasMoreSrcs))
		&& (hasMoreSrcs == PR_TRUE))
	{
		nsCOMPtr<nsISupports>	aSrc;
		if (NS_FAILED(rv = srcList->GetNext(getter_AddRefs(aSrc))))
			break;
		nsCOMPtr<nsIRDFResource>	aSource = do_QueryInterface(aSrc);
		if (!aSource)	continue;

		if (NS_FAILED(rv = mDataSource->Unassert(aSource, kNC_FolderType, objType)))
			continue;
	}

	rv = mDataSource->Assert(newSource, kNC_FolderType, objType, PR_TRUE);

	return(rv);
}



////////////////////////////////////////////////////////////////////////
// BookmarkDataSourceImpl



class nsBookmarksService : public nsIBookmarksService,
			   public nsIRDFDataSource,
			   public nsIRDFRemoteDataSource,
			   public nsIStreamListener,
			   public nsIRDFObserver
{
protected:
	nsIRDFDataSource*		mInner;
	PRBool				mBookmarksAvailable;
	PRBool				mDirty;
	PRBool				busySchedule;
	nsCOMPtr<nsIRDFResource>	busyResource;
	PRUint32			htmlSize;
	nsCOMPtr<nsISupportsArray>      mObservers;
	nsCOMPtr<nsIStringBundle>	mBundle;
	nsString			mPersonalToolbarName;
static	nsCOMPtr<nsITimer>		mTimer;

#ifdef	XP_MAC
	PRBool				mIEFavoritesAvailable;

	nsresult ReadFavorites();
#endif

static	void	FireTimer(nsITimer* aTimer, void* aClosure);
nsresult	ExamineBookmarkSchedule(nsIRDFResource *theBookmark, PRBool & examineFlag);
nsresult	GetBookmarkToPing(nsIRDFResource **theBookmark);

	nsresult GetBookmarksFile(nsFileSpec* aResult);
	nsresult WriteBookmarks(nsFileSpec *bookmarksFile, nsIRDFDataSource *ds, nsIRDFResource *root);
	nsresult WriteBookmarksContainer(nsIRDFDataSource *ds, nsOutputFileStream strm, nsIRDFResource *container, PRInt32 level, nsISupportsArray *parentArray);
	nsresult GetTextForNode(nsIRDFNode* aNode, nsString& aResult);
	nsresult UpdateBookmarkLastModifiedDate(nsIRDFResource *aSource);
	nsresult WriteBookmarkProperties(nsIRDFDataSource *ds, nsOutputFileStream strm, nsIRDFResource *node,
					 nsIRDFResource *property, const char *htmlAttrib, PRBool isFirst);
	PRBool   CanAccept(nsIRDFResource* aSource, nsIRDFResource* aProperty, nsIRDFNode* aTarget);

	nsresult getArgumentN(nsISupportsArray *arguments, nsIRDFResource *res, PRInt32 offset, nsIRDFNode **argValue);
	nsresult insertBookmarkItem(nsIRDFResource *src, nsISupportsArray *aArguments, PRInt32 parentArgIndex, nsIRDFResource *objType);
	nsresult deleteBookmarkItem(nsIRDFResource *src, nsISupportsArray *aArguments, PRInt32 parentArgIndex, nsIRDFResource *objType);
	nsresult setFolderHint(nsIRDFResource *src, nsIRDFResource *objType);
	nsresult getFolderViaHint(nsIRDFResource *src, PRBool fallbackFlag, nsIRDFResource **folder);
	nsresult importBookmarks(nsISupportsArray *aArguments);
	nsresult exportBookmarks(nsISupportsArray *aArguments);

	nsresult getResourceFromLiteralNode(nsIRDFNode *node, nsIRDFResource **res);

	nsresult ChangeURL(nsIRDFResource* aOldURL,
                           nsIRDFResource* aNewURL);

	nsresult getLocaleString(const char *key, nsString &str);

	// nsIStreamObserver methods:
	NS_DECL_NSISTREAMOBSERVER

	// nsIStreamListener methods:
	NS_DECL_NSISTREAMLISTENER

public:
	nsBookmarksService();
	virtual ~nsBookmarksService();
	nsresult Init();

	// nsISupports
	NS_DECL_ISUPPORTS

	// nsIBookmarksService
	NS_DECL_NSIBOOKMARKSSERVICE

	// nsIRDFDataSource
	NS_IMETHOD GetURI(char* *uri);

	NS_IMETHOD GetSource(nsIRDFResource* property,
			     nsIRDFNode* target,
			     PRBool tv,
			     nsIRDFResource** source) {
		return mInner->GetSource(property, target, tv, source);
	}

	NS_IMETHOD GetSources(nsIRDFResource* property,
			      nsIRDFNode* target,
			      PRBool tv,
			      nsISimpleEnumerator** sources) {
		return mInner->GetSources(property, target, tv, sources);
	}

	NS_IMETHOD GetTarget(nsIRDFResource* source,
			     nsIRDFResource* property,
			     PRBool tv,
			     nsIRDFNode** target);

	NS_IMETHOD GetTargets(nsIRDFResource* source,
			      nsIRDFResource* property,
			      PRBool tv,
			      nsISimpleEnumerator** targets) {
		return mInner->GetTargets(source, property, tv, targets);
	}

	NS_IMETHOD Assert(nsIRDFResource* aSource,
			  nsIRDFResource* aProperty,
			  nsIRDFNode* aTarget,
			  PRBool aTruthValue);

	NS_IMETHOD Unassert(nsIRDFResource* aSource,
			    nsIRDFResource* aProperty,
			    nsIRDFNode* aTarget);

	NS_IMETHOD Change(nsIRDFResource* aSource,
			  nsIRDFResource* aProperty,
			  nsIRDFNode* aOldTarget,
			  nsIRDFNode* aNewTarget);

	NS_IMETHOD Move(nsIRDFResource* aOldSource,
			nsIRDFResource* aNewSource,
			nsIRDFResource* aProperty,
			nsIRDFNode* aTarget);

	NS_IMETHOD HasAssertion(nsIRDFResource* source,
				nsIRDFResource* property,
				nsIRDFNode* target,
				PRBool tv,
				PRBool* hasAssertion) {
		return mInner->HasAssertion(source, property, target, tv, hasAssertion);
	}

	NS_IMETHOD AddObserver(nsIRDFObserver* aObserver);
	NS_IMETHOD RemoveObserver(nsIRDFObserver* aObserver);

	NS_IMETHOD ArcLabelsIn(nsIRDFNode* node,
			       nsISimpleEnumerator** labels) {
		return mInner->ArcLabelsIn(node, labels);
	}

	NS_IMETHOD ArcLabelsOut(nsIRDFResource* source,
				nsISimpleEnumerator** labels)
	{
#ifdef	XP_MAC

		// on the Mac, IE favorites are stored in an HTML file.
		// Defer importing this files contents until necessary.

		if ((source == kNC_IEFavoritesRoot) && (mIEFavoritesAvailable == PR_FALSE))
		{
			ReadFavorites();
		}
#endif
		return mInner->ArcLabelsOut(source, labels);
	}

	NS_IMETHOD GetAllResources(nsISimpleEnumerator** aResult)
	{
#ifdef	XP_MAC
		if (mIEFavoritesAvailable == PR_FALSE)
		{
			ReadFavorites();
		}
#endif
		return mInner->GetAllResources(aResult);
	}

	NS_IMETHOD GetAllCommands(nsIRDFResource* source,
				  nsIEnumerator/*<nsIRDFResource>*/** commands);

	NS_IMETHOD GetAllCmds(nsIRDFResource* source,
                              nsISimpleEnumerator/*<nsIRDFResource>*/** commands);

	NS_IMETHOD IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
				    nsIRDFResource*   aCommand,
				    nsISupportsArray/*<nsIRDFResource>*/* aArguments,
				    PRBool* aResult);

	NS_IMETHOD DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
			     nsIRDFResource*   aCommand,
			     nsISupportsArray/*<nsIRDFResource>*/* aArguments);

	// nsIRDFRemoteDataSource
	NS_DECL_NSIRDFREMOTEDATASOURCE

	// nsIRDFObserver
	NS_DECL_NSIRDFOBSERVER
};



////////////////////////////////////////////////////////////////////////



nsCOMPtr<nsITimer>		nsBookmarksService::mTimer;



nsBookmarksService::nsBookmarksService()
	: mInner(nsnull), mBookmarksAvailable(PR_FALSE), mDirty(PR_FALSE)
#ifdef	XP_MAC
	,mIEFavoritesAvailable(PR_FALSE)
#endif
{
	NS_INIT_REFCNT();
}



nsBookmarksService::~nsBookmarksService()
{
	if (mTimer)
	{
		// be sure to cancel the timer, as it holds a
		// weak reference back to nsBookmarksService
		mTimer->Cancel();
		mTimer = nsnull;
	}
	// Note: can't flush in the DTOR, as the RDF service
	// has probably already been destroyed
	// Flush();
	bm_ReleaseGlobals();
	NS_IF_RELEASE(mInner);
}



nsresult
nsBookmarksService::Init()
{
	nsresult rv;
	rv = bm_AddRefGlobals();
	if (NS_FAILED(rv))	return(rv);

	// register this as a named data source with the RDF service
	rv = gRDF->RegisterDataSource(this, PR_FALSE);
	if (NS_FAILED(rv)) return rv;

	/* create a URL for the string resource file */
	nsCOMPtr<nsIIOService>	pNetService;
	if (NS_SUCCEEDED(rv = nsServiceManager::GetService(kIOServiceCID, NS_GET_IID(nsIIOService),
		getter_AddRefs(pNetService))))
	{
		nsCOMPtr<nsIURI>	uri;
		if (NS_SUCCEEDED(rv = pNetService->NewURI(bookmark_properties, nsnull,
			getter_AddRefs(uri))))
		{
			/* create a bundle for the localization */
			nsCOMPtr<nsIStringBundleService>	stringService;
			if (NS_SUCCEEDED(rv = nsServiceManager::GetService(kStringBundleServiceCID,
				NS_GET_IID(nsIStringBundleService), getter_AddRefs(stringService))))
			{
				char	*spec = nsnull;
				if (NS_SUCCEEDED(rv = uri->GetSpec(&spec)) && (spec))
				{
					nsCOMPtr<nsILocale>		locale = nsnull;
					if (NS_SUCCEEDED(rv = stringService->CreateBundle(spec,
						locale, getter_AddRefs(mBundle))))
					{
					}
					nsCRT::free(spec);
					spec = nsnull;
				}
			}
		}
	}

	// determine what the name of the Personal Toolbar Folder is...
	// first from user preference, then string bundle, then hard-coded default
	NS_WITH_SERVICE(nsIPref, prefServ, kPrefCID, &rv);
	if (NS_SUCCEEDED(rv) && (prefServ))
	{
		char	*prefVal = nsnull;
		if (NS_SUCCEEDED(rv = prefServ->CopyCharPref("custtoolbar.personal_toolbar_folder",
			&prefVal)) && (prefVal))
		{
			if (*prefVal)
			{
				mPersonalToolbarName.AssignWithConversion(prefVal);
#ifdef	DEBUG
				printf("Obtained name of Personal Toolbar from user preferences.\n");
#endif
			}
			nsCRT::free(prefVal);
			prefVal = nsnull;
		}

		if (mPersonalToolbarName.Length() == 0)
		{
			// rjc note: always try to get the string bundle (see above) before trying this
			getLocaleString("DefaultPersonalToolbarFolder", mPersonalToolbarName);
#ifdef	DEBUG
			printf("Obtained name of Personal Toolbar from bookmarks string bundle.\n");
#endif
		}

		if (mPersonalToolbarName.Length() == 0)
		{
			// no preference, so fallback to a well-known name
			mPersonalToolbarName.AssignWithConversion(kDefaultPersonalToolbarFolder);
#ifdef	DEBUG
			printf("Obtained name of Personal Toolbar from fallback hard-coded string.\n");
#endif
		}
	}

	// read in bookmarks AFTER trying to get string bundle
	rv = ReadBookmarks();
	if (NS_FAILED(rv))	return(rv);

	/* timer initialization */
	busyResource = nsnull;

	if (!mTimer)
	{
		busySchedule = PR_FALSE;

		rv = NS_NewTimer(getter_AddRefs(mTimer));
		if (NS_FAILED(rv)) return rv;
		mTimer->Init(nsBookmarksService::FireTimer, this, BOOKMARK_TIMEOUT, NS_PRIORITY_LOWEST, NS_TYPE_REPEATING_SLACK);
		// Note: don't addref "this" as we'll cancel the timer in the nsBookmarkService destructor
	}

	return NS_OK;
}



nsresult
nsBookmarksService::getLocaleString(const char *key, nsString &str)
{
	PRUnichar	*keyUni = nsnull;
	nsAutoString	keyStr; keyStr.AssignWithConversion(key);
	nsresult	rv;
	if (mBundle && (NS_SUCCEEDED(rv = mBundle->GetStringFromName(keyStr.GetUnicode(), &keyUni)))
		&& (keyUni))
	{
		str = keyUni;
		nsCRT::free(keyUni);
	}
	else
	{
		str.Truncate();
	}
	return(rv);
}



nsresult
nsBookmarksService::ExamineBookmarkSchedule(nsIRDFResource *theBookmark, PRBool & examineFlag)
{
	examineFlag = PR_FALSE;
	
	nsresult	rv = NS_OK;

	nsCOMPtr<nsIRDFNode>	scheduleNode;
	if (NS_FAILED(rv = mInner->GetTarget(theBookmark, kWEB_Schedule, PR_TRUE,
		getter_AddRefs(scheduleNode))) || (rv == NS_RDF_NO_VALUE))
		return(rv);
	
	nsCOMPtr<nsIRDFLiteral>	scheduleLiteral = do_QueryInterface(scheduleNode);
	if (!scheduleLiteral)	return(NS_ERROR_NO_INTERFACE);
	
	const PRUnichar		*scheduleUni = nsnull;
	if (NS_FAILED(rv = scheduleLiteral->GetValueConst(&scheduleUni)))
		return(rv);
	if (!scheduleUni)	return(NS_ERROR_NULL_POINTER);

	nsAutoString		schedule(scheduleUni);
	if (schedule.Length() < 1)	return(NS_ERROR_UNEXPECTED);

	// convert the current date/time from microseconds (PRTime) to seconds
	// Note: don't change now64, as its used later in the function
	PRTime		now64 = PR_Now(), temp64, million;
	LL_I2L(million, PR_USEC_PER_SEC);
	LL_DIV(temp64, now64, million);
	PRInt32		now32;
	LL_L2I(now32, temp64);

	PRExplodedTime	nowInfo;
	PR_ExplodeTime(now64, PR_LocalTimeParameters, &nowInfo);
	
	// XXX Do we need to do this?
	PR_NormalizeTime(&nowInfo, PR_LocalTimeParameters);

	nsAutoString	dayNum;
	dayNum.AppendInt(nowInfo.tm_wday, 10);

	// a schedule string has the following format:
	// Check Monday, Tuesday, and Friday | 9 AM thru 5 PM | every five minutes | change bookmark icon
	// 125|9-17|5|icon

	nsAutoString	notificationMethod;
	PRInt32		startHour = -1, endHour = -1, duration = -1;

	// should we be checking today?
	PRInt32		slashOffset;
	if ((slashOffset = schedule.FindChar(PRUnichar('|'))) >= 0)
	{
		nsAutoString	daySection;
		schedule.Left(daySection, slashOffset);
		schedule.Cut(0, slashOffset+1);
		if (daySection.Find(dayNum) >= 0)
		{
			// ok, we should be checking today.  Within hour range?
			if ((slashOffset = schedule.FindChar(PRUnichar('|'))) >= 0)
			{
				nsAutoString	hourRange;
				schedule.Left(hourRange, slashOffset);
				schedule.Cut(0, slashOffset+1);

				// now have the "hour-range" segment of the string
				// such as "9-17" or "9-12" from the examples above
				PRInt32		dashOffset;
				if ((dashOffset = hourRange.FindChar(PRUnichar('-'))) >= 1)
				{
					nsAutoString	startStr, endStr;

					hourRange.Right(endStr, hourRange.Length() - dashOffset - 1);
					hourRange.Left(startStr, dashOffset);

					PRInt32		errorCode2 = 0;
					startHour = startStr.ToInteger(&errorCode2);
					if (errorCode2)	startHour = -1;
					endHour = endStr.ToInteger(&errorCode2);
					if (errorCode2)	endHour = -1;
					
					if ((startHour >=0) && (endHour >=0))
					{
						if ((slashOffset = schedule.FindChar(PRUnichar('|'))) >= 0)
						{
							nsAutoString	durationStr;
							schedule.Left(durationStr, slashOffset);
							schedule.Cut(0, slashOffset+1);

							// get duration
							PRInt32		errorCode = 0;
							duration = durationStr.ToInteger(&errorCode);
							if (errorCode)	duration = -1;
							
							// what's left is the notification options
							notificationMethod = schedule;
						}
					}
				}
			}
		}
	}
		

#ifdef	DEBUG_BOOKMARK_PING_OUTPUT
	char *methodStr = notificationMethod.ToNewCString();
	if (methodStr)
	{
		printf("Start Hour: %d    End Hour: %d    Duration: %d mins    Method: '%s'\n",
			startHour, endHour, duration, methodStr);
		delete [] methodStr;
		methodStr = nsnull;
	}
#endif

	if ((startHour <= nowInfo.tm_hour) && (endHour >= nowInfo.tm_hour) &&
		(duration >= 1) && (notificationMethod.Length() > 0))
	{
		// OK, we're with the start/end time range, check the duration
		// against the last time we've "pinged" the server (if ever)

		examineFlag = PR_TRUE;

		nsCOMPtr<nsIRDFNode>	pingNode;
		if (NS_SUCCEEDED(rv = mInner->GetTarget(theBookmark, kWEB_LastPingDate,
			PR_TRUE, getter_AddRefs(pingNode))) && (rv != NS_RDF_NO_VALUE))
		{
			nsCOMPtr<nsIRDFDate>	pingLiteral = do_QueryInterface(pingNode);
			if (pingLiteral)
			{
				PRInt64		lastPing;
				if (NS_SUCCEEDED(rv = pingLiteral->GetValue(&lastPing)))
				{
					PRInt64		diff64, sixty;
					LL_SUB(diff64, now64, lastPing);
					
					// convert from milliseconds to seconds
					LL_DIV(diff64, diff64, million);
					// convert from seconds to minutes
					LL_I2L(sixty, 60L);
					LL_DIV(diff64, diff64, sixty);

					PRInt32		diff32;
					LL_L2I(diff32, diff64);
					if (diff32 < duration)
					{
						examineFlag = PR_FALSE;

#ifdef	DEBUG_BOOKMARK_PING_OUTPUT
						printf("Skipping URL, its too soon.\n");
#endif
					}
				}
			}
		}
	}
	return(rv);
}



nsresult
nsBookmarksService::GetBookmarkToPing(nsIRDFResource **theBookmark)
{
	nsresult	rv = NS_OK;

	*theBookmark = nsnull;

	nsCOMPtr<nsISimpleEnumerator>	srcList;
	if (NS_FAILED(rv = GetSources(kRDF_type, kNC_Bookmark, PR_TRUE, getter_AddRefs(srcList))))
		return(rv);

	nsCOMPtr<nsISupportsArray>	bookmarkList;
	if (NS_FAILED(rv = NS_NewISupportsArray(getter_AddRefs(bookmarkList))))
		return(rv);

	// build up a list of potential bookmarks to check
	PRBool	hasMoreSrcs = PR_TRUE;
	while(NS_SUCCEEDED(rv = srcList->HasMoreElements(&hasMoreSrcs))
		&& (hasMoreSrcs == PR_TRUE))
	{
		nsCOMPtr<nsISupports>	aSrc;
		if (NS_FAILED(rv = srcList->GetNext(getter_AddRefs(aSrc))))
			break;
		nsCOMPtr<nsIRDFResource>	aSource = do_QueryInterface(aSrc);
		if (!aSource)	continue;

		// does the bookmark have a schedule, and if so,
		// are we within its bounds for checking the URL?

		PRBool	examineFlag = PR_FALSE;
		if (NS_FAILED(rv = ExamineBookmarkSchedule(aSource, examineFlag))
			|| (examineFlag == PR_FALSE))	continue;

		bookmarkList->AppendElement(aSource);
	}

	// pick a random entry from the list of bookmarks to check
	PRUint32	numBookmarks;
	if (NS_SUCCEEDED(rv = bookmarkList->Count(&numBookmarks)) && (numBookmarks > 0))
	{
		PRInt32		randomNum;
		LL_L2I(randomNum, PR_Now());
		PRUint32	randomBookmark = (numBookmarks-1) % randomNum;

		nsCOMPtr<nsISupports>	iSupports;
		if (NS_SUCCEEDED(rv = bookmarkList->GetElementAt(randomBookmark,
			getter_AddRefs(iSupports))))
		{
			nsCOMPtr<nsIRDFResource>	aBookmark = do_QueryInterface(iSupports);
			if (aBookmark)
			{
				*theBookmark = aBookmark;
				NS_ADDREF(*theBookmark);
			}
		}
	}
	return(rv);
}



void
nsBookmarksService::FireTimer(nsITimer* aTimer, void* aClosure)
{
	nsBookmarksService *bmks = NS_STATIC_CAST(nsBookmarksService *, aClosure);
	if (!bmks)	return;

	if ((bmks->mBookmarksAvailable == PR_TRUE) && (bmks->mDirty == PR_TRUE))
	{
		bmks->Flush();
	}

	if (bmks->busySchedule == PR_FALSE)
	{
		nsresult			rv;
		nsCOMPtr<nsIRDFResource>	bookmark;
		if (NS_SUCCEEDED(rv = bmks->GetBookmarkToPing(getter_AddRefs(bookmark))) && (bookmark))
		{
			bmks->busyResource = bookmark;
			const char		*url = nsnull;
			bookmark->GetValueConst(&url);

#ifdef	DEBUG_BOOKMARK_PING_OUTPUT
			printf("nsBookmarksService::FireTimer - Pinging '%s'\n", url);
#endif

			nsCOMPtr<nsIURI>	uri;
			if (NS_SUCCEEDED(rv = NS_NewURI(getter_AddRefs(uri), url)))
			{
				nsCOMPtr<nsIChannel>	channel;
				if (NS_SUCCEEDED(rv = NS_OpenURI(getter_AddRefs(channel), uri, nsnull)))
				{
					channel->SetLoadAttributes(nsIChannel::FORCE_VALIDATION | nsIChannel::VALIDATE_ALWAYS);
					nsCOMPtr<nsIHTTPChannel>	httpChannel = do_QueryInterface(channel);
					if (httpChannel)
					{
						bmks->htmlSize = 0;

//						httpChannel->SetRequestMethod(HM_GET);
						httpChannel->SetRequestMethod(HM_HEAD);
						if (NS_SUCCEEDED(rv = channel->AsyncRead(bmks, nsnull)))
						{
							bmks->busySchedule = TRUE;
						}
					}
				}
			}
		}
	}
#ifdef	DEBUG_BOOKMARK_PING_OUTPUT
else
	{
	printf("nsBookmarksService::FireTimer - busy pinging.\n");
	}
#endif

#ifndef	REPEATING_TIMERS
	if (mTimer)
	{
		mTimer->Cancel();
		mTimer = nsnull;
	}
	nsresult rv = NS_NewTimer(getter_AddRefs(mTimer));
	if (NS_FAILED(rv) || (!mTimer)) return;
	mTimer->Init(nsBookmarksService::FireTimer, bmks, BOOKMARK_TIMEOUT, NS_PRIORITY_LOWEST, NS_TYPE_REPEATING_SLACK);
	// Note: don't addref "this" as we'll cancel the timer in the nsBookmarkService destructor
#endif
}



// stream observer methods



NS_IMETHODIMP
nsBookmarksService::OnStartRequest(nsIChannel* channel, nsISupports *ctxt)
{
	return(NS_OK);
}



NS_IMETHODIMP
nsBookmarksService::OnDataAvailable(nsIChannel* channel, nsISupports *ctxt, nsIInputStream *aIStream,
					  PRUint32 sourceOffset, PRUint32 aLength)
{
	// calculate html page size if server doesn't tell us in headers
	htmlSize += aLength;

	return(NS_OK);
}



NS_IMETHODIMP
nsBookmarksService::OnStopRequest(nsIChannel* channel, nsISupports *ctxt,
					nsresult status, const PRUnichar *errorMsg) 
{
	nsresult		rv;

	const char		*uri = nsnull;
	if (NS_SUCCEEDED(rv = busyResource->GetValueConst(&uri)) && (uri))
	{
#ifdef	DEBUG_BOOKMARK_PING_OUTPUT
		printf("Finished polling '%s'\n", uri);
#endif
	}

	nsCOMPtr<nsIHTTPChannel>	httpChannel = do_QueryInterface(channel);
	if (httpChannel)
	{
		nsAutoString			eTagValue, lastModValue, contentLengthValue;
		nsCOMPtr<nsISimpleEnumerator>	enumerator;
		if (NS_SUCCEEDED(rv = httpChannel->GetResponseHeaderEnumerator(getter_AddRefs(enumerator))))
		{
			PRBool			bMoreHeaders;

			while (NS_SUCCEEDED(rv = enumerator->HasMoreElements(&bMoreHeaders))
				&& (bMoreHeaders == PR_TRUE))
			{
				nsCOMPtr<nsISupports>   item;
				enumerator->GetNext(getter_AddRefs(item));
				nsCOMPtr<nsIHTTPHeader>	header = do_QueryInterface(item);
				NS_ASSERTION(header, "nsBookmarksService::OnStopRequest - Bad HTTP header.");
				if (header)
				{
					nsCOMPtr<nsIAtom>       headerAtom;
					header->GetField(getter_AddRefs(headerAtom));
					nsAutoString		headerStr;
					headerAtom->ToString(headerStr);

					char	*val = nsnull;
					
					if (headerStr.EqualsIgnoreCase("eTag"))
					{
						header->GetValue(&val);
						if (val)
						{
							eTagValue.AssignWithConversion(val);
							nsCRT::free(val);
						}
					}
					else if (headerStr.EqualsIgnoreCase("Last-Modified"))
					{
						header->GetValue(&val);
						if (val)
						{
							lastModValue.AssignWithConversion(val);
							nsCRT::free(val);
						}
					}
					else if (headerStr.EqualsIgnoreCase("Content-Length"))
					{
						header->GetValue(&val);
						if (val)
						{
							contentLengthValue.AssignWithConversion(val);
							nsCRT::free(val);
						}
					}
				}
			}
		}

		PRBool		changedFlag = PR_FALSE;

		PRUint32	respStatus;
		if (NS_SUCCEEDED(rv = httpChannel->GetResponseStatus(&respStatus)))
		{
			if ((respStatus >= 200) && (respStatus <= 299))
			{
				if (eTagValue.Length() > 0)
				{
#ifdef	DEBUG_BOOKMARK_PING_OUTPUT
					const char *eTagVal = nsCAutoString(eTagValue);
					printf("eTag: '%s'\n", eTagVal);
#endif
					nsCOMPtr<nsIRDFNode>	currentETagNode;
					if (NS_SUCCEEDED(rv = mInner->GetTarget(busyResource, kWEB_LastPingETag,
						PR_TRUE, getter_AddRefs(currentETagNode))) && (rv != NS_RDF_NO_VALUE))
					{
						nsCOMPtr<nsIRDFLiteral>	currentETagLit = do_QueryInterface(currentETagNode);
						if (currentETagLit)
						{
							const PRUnichar	*currentETagStr = nsnull;
							currentETagLit->GetValueConst(&currentETagStr);
							if ((currentETagStr) && (!eTagValue.EqualsIgnoreCase(currentETagStr)))
							{
								changedFlag = PR_TRUE;
							}
							nsCOMPtr<nsIRDFLiteral>	newETagLiteral;
							if (NS_SUCCEEDED(rv = gRDF->GetLiteral(eTagValue.GetUnicode(),
								getter_AddRefs(newETagLiteral))))
							{
								rv = mInner->Change(busyResource, kWEB_LastPingETag,
									currentETagNode, newETagLiteral);
							}
						}
					}
					else
					{
						nsCOMPtr<nsIRDFLiteral>	newETagLiteral;
						if (NS_SUCCEEDED(rv = gRDF->GetLiteral(eTagValue.GetUnicode(),
							getter_AddRefs(newETagLiteral))))
						{
							rv = mInner->Assert(busyResource, kWEB_LastPingETag,
								newETagLiteral, PR_TRUE);
						}
					}
				}
			}
		}

		if ((changedFlag == PR_FALSE) && (lastModValue.Length() > 0))
		{
#ifdef	DEBUG_BOOKMARK_PING_OUTPUT
			const char *lastModVal = nsCAutoString(lastModValue);
			printf("Last-Modified: '%s'\n", lastModVal);
#endif
			nsCOMPtr<nsIRDFNode>	currentLastModNode;
			if (NS_SUCCEEDED(rv = mInner->GetTarget(busyResource, kWEB_LastPingModDate,
				PR_TRUE, getter_AddRefs(currentLastModNode))) && (rv != NS_RDF_NO_VALUE))
			{
				nsCOMPtr<nsIRDFLiteral>	currentLastModLit = do_QueryInterface(currentLastModNode);
				if (currentLastModLit)
				{
					const PRUnichar	*currentLastModStr = nsnull;
					currentLastModLit->GetValueConst(&currentLastModStr);
					if ((currentLastModStr) && (!lastModValue.EqualsIgnoreCase(currentLastModStr)))
					{
						changedFlag = PR_TRUE;
					}
					nsCOMPtr<nsIRDFLiteral>	newLastModLiteral;
					if (NS_SUCCEEDED(rv = gRDF->GetLiteral(lastModValue.GetUnicode(),
						getter_AddRefs(newLastModLiteral))))
					{
						rv = mInner->Change(busyResource, kWEB_LastPingModDate,
							currentLastModNode, newLastModLiteral);
					}
				}
			}
			else
			{
				nsCOMPtr<nsIRDFLiteral>	newLastModLiteral;
				if (NS_SUCCEEDED(rv = gRDF->GetLiteral(lastModValue.GetUnicode(),
					getter_AddRefs(newLastModLiteral))))
				{
					rv = mInner->Assert(busyResource, kWEB_LastPingModDate,
						newLastModLiteral, PR_TRUE);
				}
			}
		}

		if ((changedFlag == PR_FALSE) && (contentLengthValue.Length() > 0))
		{
#ifdef	DEBUG_BOOKMARK_PING_OUTPUT
			const char *contentLengthVal = nsCAutoString(contentLengthValue);
			printf("Content-Length: '%s'\n", contentLengthVal);
#endif
			nsCOMPtr<nsIRDFNode>	currentContentLengthNode;
			if (NS_SUCCEEDED(rv = mInner->GetTarget(busyResource, kWEB_LastPingContentLen,
				PR_TRUE, getter_AddRefs(currentContentLengthNode))) && (rv != NS_RDF_NO_VALUE))
			{
				nsCOMPtr<nsIRDFLiteral>	currentContentLengthLit = do_QueryInterface(currentContentLengthNode);
				if (currentContentLengthLit)
				{
					const PRUnichar	*currentContentLengthStr = nsnull;
					currentContentLengthLit->GetValueConst(&currentContentLengthStr);
					if ((currentContentLengthStr) && (!contentLengthValue.EqualsIgnoreCase(currentContentLengthStr)))
					{
						changedFlag = PR_TRUE;
					}
					nsCOMPtr<nsIRDFLiteral>	newContentLengthLiteral;
					if (NS_SUCCEEDED(rv = gRDF->GetLiteral(contentLengthValue.GetUnicode(),
						getter_AddRefs(newContentLengthLiteral))))
					{
						rv = mInner->Change(busyResource, kWEB_LastPingContentLen,
							currentContentLengthNode, newContentLengthLiteral);
					}
				}
			}
			else
			{
				nsCOMPtr<nsIRDFLiteral>	newContentLengthLiteral;
				if (NS_SUCCEEDED(rv = gRDF->GetLiteral(contentLengthValue.GetUnicode(),
					getter_AddRefs(newContentLengthLiteral))))
				{
					rv = mInner->Assert(busyResource, kWEB_LastPingContentLen,
						newContentLengthLiteral, PR_TRUE);
				}
			}
		}

		// update last poll date
		nsCOMPtr<nsIRDFDate>	dateLiteral;
		if (NS_SUCCEEDED(rv = gRDF->GetDateLiteral(PR_Now(), getter_AddRefs(dateLiteral))))
		{
			nsCOMPtr<nsIRDFNode>	lastPingNode;
			if (NS_SUCCEEDED(rv = mInner->GetTarget(busyResource, kWEB_LastPingDate, PR_TRUE,
				getter_AddRefs(lastPingNode))) && (rv != NS_RDF_NO_VALUE))
			{
				rv = mInner->Change(busyResource, kWEB_LastPingDate, lastPingNode, dateLiteral);
			}
			else
			{
				rv = mInner->Assert(busyResource, kWEB_LastPingDate, dateLiteral, PR_TRUE);
			}
			NS_ASSERTION(rv == NS_RDF_ASSERTION_ACCEPTED, "unable to assert new time");

//			mDirty = PR_TRUE;
		}
		else
		{
			NS_ERROR("unable to get date literal for now");
		}

		// If its changed, set the appropriate info
		if (changedFlag == PR_TRUE)
		{
#ifdef	DEBUG_BOOKMARK_PING_OUTPUT
			printf("URL has changed!\n\n");
#endif

			nsAutoString		schedule;

			nsCOMPtr<nsIRDFNode>	scheduleNode;
			if (NS_SUCCEEDED(rv = mInner->GetTarget(busyResource, kWEB_Schedule, PR_TRUE,
				getter_AddRefs(scheduleNode))) && (rv != NS_RDF_NO_VALUE))
			{
				nsCOMPtr<nsIRDFLiteral>	scheduleLiteral = do_QueryInterface(scheduleNode);
				if (scheduleLiteral)
				{
					const PRUnichar		*scheduleUni = nsnull;
					if (NS_SUCCEEDED(rv = scheduleLiteral->GetValueConst(&scheduleUni))
						&& (scheduleUni))
					{
						schedule = scheduleUni;
					}
				}
			}

			// update icon?
			if (schedule.Find(NS_ConvertASCIItoUCS2("icon"), PR_TRUE, 0) >= 0)
			{
				nsCOMPtr<nsIRDFLiteral>	statusLiteral;
				if (NS_SUCCEEDED(rv = gRDF->GetLiteral(NS_ConvertASCIItoUCS2("new").GetUnicode(), getter_AddRefs(statusLiteral))))
				{
					nsCOMPtr<nsIRDFNode>	currentStatusNode;
					if (NS_SUCCEEDED(rv = mInner->GetTarget(busyResource, kWEB_Status, PR_TRUE,
						getter_AddRefs(currentStatusNode))) && (rv != NS_RDF_NO_VALUE))
					{
						rv = mInner->Change(busyResource, kWEB_Status, currentStatusNode, statusLiteral);
					}
					else
					{
						rv = mInner->Assert(busyResource, kWEB_Status, statusLiteral, PR_TRUE);
					}
					NS_ASSERTION(rv == NS_RDF_ASSERTION_ACCEPTED, "unable to assert changed status");
				}
			}
			
			// play a sound?
			if (schedule.Find(NS_ConvertASCIItoUCS2("sound"), PR_TRUE, 0) >= 0)
			{
/*
				nsCOMPtr<nsISound>	soundInterface;
				rv = nsComponentManager::CreateInstance(kSoundCID,
						nsnull, NS_GET_IID(nsISound),
						getter_AddRefs(soundInterface));
				if (NS_SUCCEEDED(rv))
				{
					// XXX for the moment, just beep
					soundInterface->Beep();
				}
*/
			}
			
			PRBool		openURLFlag = PR_FALSE;

			// show an alert?
			if (schedule.Find(NS_ConvertASCIItoUCS2("alert"), PR_TRUE, 0) >= 0)
			{
				NS_WITH_SERVICE(nsIPrompt, dialog, kNetSupportDialogCID, &rv);
				if (NS_SUCCEEDED(rv))
				{
					nsAutoString	promptStr;
					getLocaleString("WebPageUpdated", promptStr);
					if (promptStr.Length() > 0)	promptStr.AppendWithConversion("\n\n");

					nsCOMPtr<nsIRDFNode>	nameNode;
					if (NS_SUCCEEDED(mInner->GetTarget(busyResource, kNC_Name,
						PR_TRUE, getter_AddRefs(nameNode))))
					{
						nsCOMPtr<nsIRDFLiteral>	nameLiteral = do_QueryInterface(nameNode);
						if (nameLiteral)
						{
							const PRUnichar	*nameUni = nsnull;
							if (NS_SUCCEEDED(rv = nameLiteral->GetValueConst(&nameUni))
								&& (nameUni))
							{
								nsAutoString	info;
								getLocaleString("WebPageTitle", info);
								promptStr += info;
								promptStr.AppendWithConversion(" ");
								promptStr += nameUni;
								promptStr.AppendWithConversion("\n");
								getLocaleString("WebPageURL", info);
								promptStr += info;
								promptStr.AppendWithConversion(" ");
							}
						}
					}
					promptStr.AppendWithConversion(uri);
					
					nsAutoString	temp;
					getLocaleString("WebPageAskDisplay", temp);
					if (temp.Length() > 0)
					{
						promptStr.AppendWithConversion("\n\n");
						promptStr += temp;
					}

					nsAutoString	stopOption;
					getLocaleString("WebPageAskStopOption", stopOption);
					PRBool		stopCheckingFlag = PR_FALSE;
					rv = dialog->ConfirmCheck(promptStr.GetUnicode(), stopOption.GetUnicode(),
						&stopCheckingFlag, &openURLFlag);
					if (NS_FAILED(rv))
					{
						openURLFlag = PR_FALSE;
						stopCheckingFlag = PR_FALSE;
					}
					if (stopCheckingFlag == PR_TRUE)
					{
#ifdef	DEBUG_BOOKMARK_PING_OUTPUT
						printf("\nStop checking this URL.\n");
#endif
						rv = mInner->Unassert(busyResource, kWEB_Schedule, scheduleNode);
						NS_ASSERTION(NS_SUCCEEDED(rv), "unable to unassert kWEB_Schedule");
					}
				}
			}
			
			// open the URL in a new window?
			if ((openURLFlag == PR_TRUE) ||
				(schedule.Find(NS_ConvertASCIItoUCS2("open"), PR_TRUE, 0) >= 0))
			{
				NS_WITH_SERVICE(nsIAppShellService, appShell, kAppShellServiceCID, &rv);
				if (NS_SUCCEEDED(rv))
				{
					// get a parent window for the new browser window
					nsCOMPtr<nsIXULWindow>	parent;
					appShell->GetHiddenWindow(getter_AddRefs(parent));

					// convert it to a DOMWindow
					nsCOMPtr<nsIDocShell>	docShell;
					if (parent)
					{
						parent->GetDocShell(getter_AddRefs(docShell));
					}
					nsCOMPtr<nsIDOMWindow>	domParent(do_GetInterface(docShell));
					nsCOMPtr<nsIScriptGlobalObject>	sgo(do_QueryInterface(domParent));

					nsCOMPtr<nsIScriptContext>	context;
					if (sgo)
					{
						sgo->GetContext(getter_AddRefs(context));
					}
					if (context)
					{
						JSContext *jsContext = (JSContext*)context->GetNativeContext();
						if (jsContext)
						{
							void	*stackPtr;
							jsval	*argv = JS_PushArguments(jsContext, &stackPtr, "s", uri);
							if (argv)
							{
					                        // open the window
					                        nsIDOMWindow	*newWindow;
					                        domParent->Open(jsContext, argv, 1, &newWindow);
					                        JS_PopArguments(jsContext, stackPtr);
							}
						}
					}
				}
			}
		}
#ifdef	DEBUG_BOOKMARK_PING_OUTPUT
		else
		{
			printf("URL has not changed status.\n\n");
		}
#endif
	}

	busyResource = null_nsCOMPtr();
	busySchedule = PR_FALSE;

	return(NS_OK);
}

////////////////////////////////////////////////////////////////////////

NS_IMPL_ADDREF(nsBookmarksService);

NS_IMETHODIMP_(nsrefcnt)
nsBookmarksService::Release()
{
	// We need a special implementation of Release() because our mInner
	// holds a Circular References back to us.
	NS_PRECONDITION(PRInt32(mRefCnt) > 0, "duplicate release");
	--mRefCnt;
	NS_LOG_RELEASE(this, mRefCnt, "nsBookmarksService");

	if (mInner && mRefCnt == 1) {
		nsIRDFDataSource* tmp = mInner;
		mInner = nsnull;
		NS_IF_RELEASE(tmp);
		return 0;
	}
	else if (mRefCnt == 0) {
		delete this;
		return 0;
	}
	else {
		return mRefCnt;
	}
}



NS_IMPL_QUERY_INTERFACE6(nsBookmarksService, 
			 nsIBookmarksService,
			 nsIRDFDataSource,
			 nsIRDFRemoteDataSource,
			 nsIRDFObserver,
			 nsIStreamListener,
			 nsIStreamObserver)

////////////////////////////////////////////////////////////////////////
// nsIBookmarksService



NS_IMETHODIMP
nsBookmarksService::AddBookmark(const char *aURI, const PRUnichar *aOptionalTitle)
{
	// XXX Constructing a parser object to do this is bad.
	// We need to factor AddBookmark() into its own little
	// routine or something.

	BookmarkParser parser;
	parser.Init(nsnull, mInner, mPersonalToolbarName);

	nsresult rv;

	nsCOMPtr<nsIRDFContainer> container;
	rv = nsComponentManager::CreateInstance(kRDFContainerCID,
						nsnull,
						NS_GET_IID(nsIRDFContainer),
						getter_AddRefs(container));
	if (NS_FAILED(rv)) return rv;

	// figure out where to add the new bookmark
	nsCOMPtr<nsIRDFResource>	bookmarkType = kNC_NewBookmarkFolder;

	// hack: route "internetsearch:" URLs if necessary
	if (!nsCRT::strncmp(aURI, "internetsearch:", 15))
	{
		bookmarkType = kNC_NewSearchFolder;
	}

	nsCOMPtr<nsIRDFResource>	newBookmarkFolder;
	if (NS_FAILED(rv = getFolderViaHint(bookmarkType, PR_TRUE,
			getter_AddRefs(newBookmarkFolder))))
		return(rv);

	rv = container->Init(mInner, newBookmarkFolder);
	if (NS_FAILED(rv)) return(rv);

	// convert the current date/time from microseconds (PRTime) to seconds
	PRTime		now64 = PR_Now(), million;
	LL_I2L(million, PR_USEC_PER_SEC);
	LL_DIV(now64, now64, million);
	PRInt32		now32;
	LL_L2I(now32, now64);

	rv = parser.AddBookmark(container, aURI, aOptionalTitle, now32,
				0L, 0L, nsnull, kNC_Bookmark, nsnull);

	if (NS_FAILED(rv)) return(rv);

	mDirty = PR_TRUE;
	Flush();

	return(NS_OK);
}



NS_IMETHODIMP
nsBookmarksService::IsBookmarked(const char *aURI, PRBool *isBookmarkedFlag)
{
	if (!aURI)		return(NS_ERROR_UNEXPECTED);
	if (!isBookmarkedFlag)	return(NS_ERROR_UNEXPECTED);
	if (!mInner)		return(NS_ERROR_UNEXPECTED);

	*isBookmarkedFlag = PR_FALSE;

	nsresult			rv;
	nsCOMPtr<nsIRDFResource>	bookmark;

	// check if it has the proper type
	if (NS_FAILED(rv = gRDF->GetResource(aURI, getter_AddRefs(bookmark))))
		return(rv);

	// make sure it is referred to by an ordinal (i.e. is contained in a rdf seq)
	nsCOMPtr<nsISimpleEnumerator>	enumerator;
	if (NS_FAILED(rv = mInner->ArcLabelsIn(bookmark, getter_AddRefs(enumerator))))
		return(rv);
		
	PRBool	more = PR_TRUE;
	while(NS_SUCCEEDED(rv = enumerator->HasMoreElements(&more))
		&& (more == PR_TRUE))
	{
		nsCOMPtr<nsISupports>		isupports;
		if (NS_FAILED(rv = enumerator->GetNext(getter_AddRefs(isupports))))
			break;
		nsCOMPtr<nsIRDFResource>	property = do_QueryInterface(isupports);
		if (!property)	continue;

		PRBool	flag = PR_FALSE;
		if (NS_FAILED(rv = gRDFC->IsOrdinalProperty(property, &flag)))	continue;
		if (flag == PR_TRUE)
		{
			*isBookmarkedFlag = PR_TRUE;
			break;
		}
	}
	return(rv);
}



NS_IMETHODIMP
nsBookmarksService::UpdateBookmarkLastVisitedDate(const char *aURL)
{
	nsCOMPtr<nsIRDFResource>	bookmark;
	nsresult			rv;

	if (NS_SUCCEEDED(rv = gRDF->GetResource(aURL, getter_AddRefs(bookmark) )))
	{
		PRBool			isBookmark = PR_FALSE;

		// Note: always use mInner!! Otherwise, could get into an infinite loop
		// due to Assert/Change calling UpdateBookmarkLastModifiedDate()

		if (NS_SUCCEEDED(rv = mInner->HasAssertion(bookmark, kRDF_type, kNC_Bookmark,
			PR_TRUE, &isBookmark)) && (isBookmark == PR_TRUE))
		{
			nsCOMPtr<nsIRDFDate>	now;

			if (NS_SUCCEEDED(rv = gRDF->GetDateLiteral(PR_Now(), getter_AddRefs(now))))
			{
				nsCOMPtr<nsIRDFNode>	lastMod;

				if (NS_SUCCEEDED(rv = mInner->GetTarget(bookmark, kWEB_LastVisitDate, PR_TRUE,
					getter_AddRefs(lastMod))) && (rv != NS_RDF_NO_VALUE))
				{
					rv = mInner->Change(bookmark, kWEB_LastVisitDate, lastMod, now);
				}
				else
				{
					rv = mInner->Assert(bookmark, kWEB_LastVisitDate, now, PR_TRUE);
				}

				// also update bookmark's "status"!
				nsCOMPtr<nsIRDFNode>	currentStatusNode;
				if (NS_SUCCEEDED(rv = mInner->GetTarget(bookmark, kWEB_Status, PR_TRUE,
					getter_AddRefs(currentStatusNode))) && (rv != NS_RDF_NO_VALUE))
				{
					rv = mInner->Unassert(bookmark, kWEB_Status, currentStatusNode);
					NS_ASSERTION(rv == NS_RDF_ASSERTION_ACCEPTED, "unable to Unassert changed status");
				}

//				mDirty = PR_TRUE;
			}
		}
	}
	return(rv);
}



nsresult
nsBookmarksService::UpdateBookmarkLastModifiedDate(nsIRDFResource *aSource)
{
	nsCOMPtr<nsIRDFDate>	now;
	nsresult		rv;

	if (NS_SUCCEEDED(rv = gRDF->GetDateLiteral(PR_Now(), getter_AddRefs(now))))
	{
		nsCOMPtr<nsIRDFNode>	lastMod;

		// Note: always use mInner!! Otherwise, could get into an infinite loop
		// due to Assert/Change calling UpdateBookmarkLastModifiedDate()

		if (NS_SUCCEEDED(rv = mInner->GetTarget(aSource, kWEB_LastModifiedDate, PR_TRUE,
			getter_AddRefs(lastMod))) && (rv != NS_RDF_NO_VALUE))
		{
			rv = mInner->Change(aSource, kWEB_LastModifiedDate, lastMod, now);
		}
		else
		{
			rv = mInner->Assert(aSource, kWEB_LastModifiedDate, now, PR_TRUE);
		}
	}
	return(rv);
}



NS_IMETHODIMP
nsBookmarksService::FindShortcut(const PRUnichar *aUserInput, char **aShortcutURL)
{
	NS_PRECONDITION(aUserInput != nsnull, "null ptr");
	if (! aUserInput)
		return NS_ERROR_NULL_POINTER;

	NS_PRECONDITION(aShortcutURL != nsnull, "null ptr");
	if (! aShortcutURL)
		return NS_ERROR_NULL_POINTER;

	nsresult rv;

	// shortcuts are always lowercased internally
	nsAutoString		shortcut(aUserInput);
	shortcut.ToLowerCase();

	nsCOMPtr<nsIRDFLiteral> literalTarget;
	rv = gRDF->GetLiteral(shortcut.GetUnicode(), getter_AddRefs(literalTarget));
	if (NS_FAILED(rv)) return rv;

	if (rv != NS_RDF_NO_VALUE)
	{
		nsCOMPtr<nsIRDFResource> source;
		rv = GetSource(kNC_ShortcutURL, literalTarget,
			       PR_TRUE, getter_AddRefs(source));

		if (NS_FAILED(rv)) return rv;

		if (rv != NS_RDF_NO_VALUE)
		{
			rv = source->GetValue(aShortcutURL);
			if (NS_FAILED(rv)) return rv;

			return NS_OK;
		}
	}

	*aShortcutURL = nsnull;
	return NS_RDF_NO_VALUE;
}



////////////////////////////////////////////////////////////////////////
// nsIRDFDataSource



NS_IMETHODIMP
nsBookmarksService::GetURI(char* *aURI)
{
	*aURI = nsXPIDLCString::Copy("rdf:bookmarks");
	if (! *aURI)
		return NS_ERROR_OUT_OF_MEMORY;

	return NS_OK;
}



static PRBool
isBookmarkCommand(nsIRDFResource *r)
{
	PRBool		isBookmarkCommandFlag = PR_FALSE;
	const char	*uri = nsnull;
	
	if (NS_SUCCEEDED(r->GetValueConst( &uri )) && (uri))
	{
		if (!strncmp(uri, kBookmarkCommand, sizeof(kBookmarkCommand) - 1))
		{
			isBookmarkCommandFlag = PR_TRUE;
		}
	}
	return(isBookmarkCommandFlag);
}



NS_IMETHODIMP
nsBookmarksService::GetTarget(nsIRDFResource* aSource,
			      nsIRDFResource* aProperty,
			      PRBool aTruthValue,
			      nsIRDFNode** aTarget)
{
	nsresult	rv;

	// If they want the URL...
	if (aTruthValue && aProperty == kNC_URL)
	{
		// ...and it is in fact a bookmark...
		PRBool hasAssertion;
		if ((NS_SUCCEEDED(mInner->HasAssertion(aSource, kRDF_type, kNC_Bookmark, PR_TRUE, &hasAssertion))
		    && hasAssertion) ||
		    (NS_SUCCEEDED(mInner->HasAssertion(aSource, kRDF_type, kNC_IEFavorite, PR_TRUE, &hasAssertion))
		    && hasAssertion) ||
		    (NS_SUCCEEDED(mInner->HasAssertion(aSource, kRDF_type, kNC_Folder, PR_TRUE, &hasAssertion))
		    && hasAssertion))
		{
			const char	*uri;
			if (NS_FAILED(rv = aSource->GetValueConst( &uri )))
			{
				NS_ERROR("unable to get source's URI");
				return rv;
			}

			nsAutoString	ncURI; ncURI.AssignWithConversion(uri);
			if (ncURI.Find("NC:", PR_TRUE, 0) == 0)
			{
				return(NS_RDF_NO_VALUE);
			}

			nsIRDFLiteral* literal;
			if (NS_FAILED(rv = gRDF->GetLiteral(NS_ConvertASCIItoUCS2(uri).GetUnicode(), &literal)))
			{
				NS_ERROR("unable to construct literal for URL");
				return rv;
			}

			*aTarget = (nsIRDFNode*)literal;
			return NS_OK;
		}
	}
	else if (aTruthValue && isBookmarkCommand(aSource) && (aProperty == kNC_Name))
	{
		nsAutoString	name;
		if (aSource == kNC_BookmarkCommand_NewBookmark)
			getLocaleString("NewBookmark", name);
		else if (aSource == kNC_BookmarkCommand_NewFolder)
			getLocaleString("NewFolder", name);
		else if (aSource == kNC_BookmarkCommand_NewSeparator)
			getLocaleString("NewSeparator", name);
		else if (aSource == kNC_BookmarkCommand_DeleteBookmark)
			getLocaleString("DeleteBookmark", name);
		else if (aSource == kNC_BookmarkCommand_DeleteBookmarkFolder)
			getLocaleString("DeleteFolder", name);
		else if (aSource == kNC_BookmarkCommand_DeleteBookmarkSeparator)
			getLocaleString("DeleteSeparator", name);
		else if (aSource == kNC_BookmarkCommand_SetNewBookmarkFolder)
			getLocaleString("SetNewBookmarkFolder", name);
		else if (aSource == kNC_BookmarkCommand_SetPersonalToolbarFolder)
			getLocaleString("SetPersonalToolbarFolder", name);
		else if (aSource == kNC_BookmarkCommand_SetNewSearchFolder)
			getLocaleString("SetNewSearchFolder", name);
		else if (aSource == kNC_BookmarkCommand_Import)
			getLocaleString("Import", name);
		else if (aSource == kNC_BookmarkCommand_Export)
			getLocaleString("Export", name);

		if (name.Length() > 0)
		{
			*aTarget = nsnull;
			nsCOMPtr<nsIRDFLiteral>	literal;
			if (NS_FAILED(rv = gRDF->GetLiteral(name.GetUnicode(), getter_AddRefs(literal))))
				return(rv);
			*aTarget = literal;
			NS_IF_ADDREF(*aTarget);
			return(rv);
		}
	}

	rv = mInner->GetTarget(aSource, aProperty, aTruthValue, aTarget);
	return(rv);
}



NS_IMETHODIMP
nsBookmarksService::Assert(nsIRDFResource* aSource,
			   nsIRDFResource* aProperty,
			   nsIRDFNode* aTarget,
			   PRBool aTruthValue)
{
	nsresult	rv = NS_RDF_ASSERTION_REJECTED;

	if (CanAccept(aSource, aProperty, aTarget))
	{
		if (aProperty == kNC_URL)
		{
			nsCOMPtr<nsIRDFResource> newURL;
			rv = getResourceFromLiteralNode(aTarget, getter_AddRefs(newURL));
			if (NS_FAILED(rv)) return rv;
			
			rv = ChangeURL(aSource, newURL);
		}
		else if (NS_SUCCEEDED(rv = mInner->Assert(aSource, aProperty, aTarget, aTruthValue)))
		{
			UpdateBookmarkLastModifiedDate(aSource);
		}
	}
	return(rv);
}



NS_IMETHODIMP
nsBookmarksService::Unassert(nsIRDFResource* aSource,
			     nsIRDFResource* aProperty,
			     nsIRDFNode* aTarget)
{
	nsresult	rv = NS_RDF_ASSERTION_REJECTED;

	if (aProperty == kNC_URL) {
		// We can't accept somebody trying to remove a URL. Sorry!
	}
	else if (CanAccept(aSource, aProperty, aTarget))
	{
		if (NS_SUCCEEDED(rv = mInner->Unassert(aSource, aProperty, aTarget)))
		{
			UpdateBookmarkLastModifiedDate(aSource);
		}
	}
	return(rv);
}



nsresult
nsBookmarksService::getResourceFromLiteralNode(nsIRDFNode *node, nsIRDFResource **res)
{
	nsresult	rv;

	nsCOMPtr<nsIRDFResource>	newURLRes = do_QueryInterface(node);
	if (newURLRes)
	{
		*res = newURLRes;
		NS_IF_ADDREF(*res);
		return(NS_OK);
	}

	nsCOMPtr<nsIRDFLiteral>		newURLLit = do_QueryInterface(node);
	if (!newURLLit)
	{
		return(NS_ERROR_INVALID_ARG);
	}
	const PRUnichar			*newURL = nsnull;
	newURLLit->GetValueConst(&newURL);
	if (!newURL)
	{
		return(NS_ERROR_NULL_POINTER);
	}
	rv = gRDF->GetUnicodeResource(newURL, res);
	return(rv);
}



nsresult
nsBookmarksService::ChangeURL(nsIRDFResource* aOldURL,
			      nsIRDFResource* aNewURL)
{
	nsresult rv;

	// Make all arcs coming out of aOldURL also come out of
	// aNewURL.  Wallop any previous values.
	
	nsCOMPtr<nsISimpleEnumerator>	arcsOut;
	rv = mInner->ArcLabelsOut(aOldURL, getter_AddRefs(arcsOut));
	if (NS_FAILED(rv)) return(rv);

	while (1)
	{
		PRBool hasMoreArcsOut;
		rv = arcsOut->HasMoreElements(&hasMoreArcsOut);
		if (NS_FAILED(rv)) return rv;

		if (! hasMoreArcsOut)
			break;

		nsCOMPtr<nsISupports> arc;
		rv = arcsOut->GetNext(getter_AddRefs(arc));
		if (NS_FAILED(rv)) return rv;

		nsCOMPtr<nsIRDFResource> property = do_QueryInterface(arc);
		NS_ASSERTION(property != nsnull, "arc is not a property");
		if (!property)
			return NS_ERROR_UNEXPECTED;

		// don't copy URL property as it is special
		if (property.get() == kNC_URL)
			continue;

		// XXX What if more than one target?
		nsCOMPtr<nsIRDFNode> oldvalue;
		rv = mInner->GetTarget(aNewURL, property, PR_TRUE, getter_AddRefs(oldvalue));
		if (NS_FAILED(rv)) return rv;

		nsCOMPtr<nsIRDFNode> newvalue;
		rv = mInner->GetTarget(aOldURL, property, PR_TRUE, getter_AddRefs(newvalue));
		if (NS_FAILED(rv)) return rv;

		if (oldvalue) {
			if (newvalue) {
				rv = mInner->Change(aNewURL, property, oldvalue, newvalue);
			}
			else {
				rv = mInner->Unassert(aNewURL, property, oldvalue);
			}
		}
		else if (newvalue) {
			rv = mInner->Assert(aNewURL, property, newvalue, PR_TRUE);
		}
		else {
			// do nothing
			rv = NS_OK;
		}

		if (NS_FAILED(rv)) return rv;
	}
	
	// Make all arcs pointing to aOldURL now point to aNewURL
	nsCOMPtr<nsISimpleEnumerator> arcsIn;
	rv = mInner->ArcLabelsIn(aOldURL, getter_AddRefs(arcsIn));
	if (NS_FAILED(rv)) return rv;

	while (1)
	{
		PRBool hasMoreArcsIn;
		rv = arcsIn->HasMoreElements(&hasMoreArcsIn);
		if (NS_FAILED(rv)) return rv;

		if (! hasMoreArcsIn)
			break;
		
		nsCOMPtr<nsIRDFResource> property;

		{
			nsCOMPtr<nsISupports> isupports;
			rv = arcsIn->GetNext(getter_AddRefs(isupports));
			if (NS_FAILED(rv)) return rv;

			property = do_QueryInterface(isupports);
			NS_ASSERTION(property != nsnull, "arc is not a property");
			if (! property)
				return NS_ERROR_UNEXPECTED;
		}

		nsCOMPtr<nsISimpleEnumerator> sources;
		rv = GetSources(property, aOldURL, PR_TRUE, getter_AddRefs(sources));
		if (NS_FAILED(rv)) return rv;
		
		while (1)
		{
			PRBool hasMoreSrcs;
			rv = sources->HasMoreElements(&hasMoreSrcs);
			if (NS_FAILED(rv)) return rv;

			if (! hasMoreSrcs)
				break;

			nsCOMPtr<nsISupports> isupports;
			rv = sources->GetNext(getter_AddRefs(isupports));
			if (NS_FAILED(rv)) return rv;

			nsCOMPtr<nsIRDFResource> source = do_QueryInterface(isupports);
			NS_ASSERTION(source != nsnull, "source is not a resource");
			if (! source)
				return NS_ERROR_UNEXPECTED;

			rv = mInner->Change(source, property, aOldURL, aNewURL);
			if (NS_FAILED(rv)) return rv;
		}
	}

	// Set a notification that the URL property changed, so that
	// anyone observing it'll update correctly.
	{
		const char* uri;
		rv = aNewURL->GetValueConst(&uri);
		if (NS_FAILED(rv)) return rv;

		nsCOMPtr<nsIRDFLiteral> literal;
		rv = gRDF->GetLiteral(NS_ConvertASCIItoUCS2(uri).GetUnicode(), getter_AddRefs(literal));
		if (NS_FAILED(rv)) return rv;

		// XXX rjc: was just aNewURL. Don't both aOldURL as well as aNewURL need to be pinged?
		rv = OnAssert(aOldURL, kNC_URL, literal);
		if (NS_FAILED(rv)) return rv;
		rv = OnAssert(aNewURL, kNC_URL, literal);
		if (NS_FAILED(rv)) return rv;
	}


	return NS_OK;
}



NS_IMETHODIMP
nsBookmarksService::Change(nsIRDFResource* aSource,
			   nsIRDFResource* aProperty,
			   nsIRDFNode* aOldTarget,
			   nsIRDFNode* aNewTarget)
{
	nsresult	rv = NS_RDF_ASSERTION_REJECTED;

	if (CanAccept(aSource, aProperty, aNewTarget))
	{
		if (aProperty == kNC_URL)
		{
			// It should be the case that aOldTarget
			// points to a literal whose value is the same
			// as aSource's URI.
			nsCOMPtr<nsIRDFResource> newURL;
			rv = getResourceFromLiteralNode(aNewTarget, getter_AddRefs(newURL));
			if (NS_FAILED(rv)) return rv;
			
			rv = ChangeURL(aSource, newURL);
		}
		else if (NS_SUCCEEDED(rv = mInner->Change(aSource, aProperty, aOldTarget, aNewTarget)))
		{
			UpdateBookmarkLastModifiedDate(aSource);
		}
	}
	return(rv);
}



NS_IMETHODIMP
nsBookmarksService::Move(nsIRDFResource* aOldSource,
			 nsIRDFResource* aNewSource,
			 nsIRDFResource* aProperty,
			 nsIRDFNode* aTarget)
{
	nsresult	rv = NS_RDF_ASSERTION_REJECTED;

	if (CanAccept(aNewSource, aProperty, aTarget))
	{
		if (NS_SUCCEEDED(rv = mInner->Move(aOldSource, aNewSource, aProperty, aTarget)))
		{
			UpdateBookmarkLastModifiedDate(aOldSource);
			UpdateBookmarkLastModifiedDate(aNewSource);
		}
	}
	return(rv);
}


NS_IMETHODIMP
nsBookmarksService::AddObserver(nsIRDFObserver* aObserver)
{
	if (! aObserver)
		return NS_ERROR_NULL_POINTER;

	if (! mObservers) {
		nsresult rv;
		rv = NS_NewISupportsArray(getter_AddRefs(mObservers));
		if (NS_FAILED(rv)) return rv;
	}

	mObservers->AppendElement(aObserver);
	return NS_OK;
}


NS_IMETHODIMP
nsBookmarksService::RemoveObserver(nsIRDFObserver* aObserver)
{
	if (! aObserver)
		return NS_ERROR_NULL_POINTER;

	if (mObservers) {
		mObservers->RemoveElement(aObserver);
	}

	return NS_OK;
}



NS_IMETHODIMP
nsBookmarksService::GetAllCommands(nsIRDFResource* source,
				   nsIEnumerator/*<nsIRDFResource>*/** commands)
{
	NS_NOTYETIMPLEMENTED("write me!");
	return NS_ERROR_NOT_IMPLEMENTED;
}



NS_IMETHODIMP
nsBookmarksService::GetAllCmds(nsIRDFResource* source,
			       nsISimpleEnumerator/*<nsIRDFResource>*/** commands)
{
	nsCOMPtr<nsISupportsArray>	cmdArray;
	nsresult			rv;
	rv = NS_NewISupportsArray(getter_AddRefs(cmdArray));
	if (NS_FAILED(rv))	return(rv);

	// determine type
	PRBool	isBookmark = PR_FALSE, isBookmarkFolder = PR_FALSE, isBookmarkSeparator = PR_FALSE;
	mInner->HasAssertion(source, kRDF_type, kNC_Bookmark, PR_TRUE, &isBookmark);
	mInner->HasAssertion(source, kRDF_type, kNC_Folder, PR_TRUE, &isBookmarkFolder);
	mInner->HasAssertion(source, kRDF_type, kNC_BookmarkSeparator, PR_TRUE, &isBookmarkSeparator);

	if (isBookmark || isBookmarkFolder || isBookmarkSeparator)
	{
		cmdArray->AppendElement(kNC_BookmarkCommand_NewBookmark);
		cmdArray->AppendElement(kNC_BookmarkCommand_NewFolder);
		cmdArray->AppendElement(kNC_BookmarkCommand_NewSeparator);
		cmdArray->AppendElement(kNC_BookmarkSeparator);
	}
	if (isBookmark)
	{
		cmdArray->AppendElement(kNC_BookmarkCommand_DeleteBookmark);
	}
	if (isBookmarkFolder && (source != kNC_BookmarksRoot) && (source != kNC_IEFavoritesRoot))
	{
		cmdArray->AppendElement(kNC_BookmarkCommand_DeleteBookmarkFolder);
	}
	if (isBookmarkSeparator)
	{
		cmdArray->AppendElement(kNC_BookmarkCommand_DeleteBookmarkSeparator);
	}
	if (isBookmarkFolder)
	{
		nsCOMPtr<nsIRDFResource>	newBookmarkFolder, personalToolbarFolder, newSearchFolder;
		getFolderViaHint(kNC_NewBookmarkFolder, PR_FALSE, getter_AddRefs(newBookmarkFolder));
		getFolderViaHint(kNC_PersonalToolbarFolder, PR_FALSE, getter_AddRefs(personalToolbarFolder));
		getFolderViaHint(kNC_NewSearchFolder, PR_FALSE, getter_AddRefs(newSearchFolder));

		cmdArray->AppendElement(kNC_BookmarkSeparator);
		if (source != newBookmarkFolder.get())		cmdArray->AppendElement(kNC_BookmarkCommand_SetNewBookmarkFolder);
		if (source != newSearchFolder.get())		cmdArray->AppendElement(kNC_BookmarkCommand_SetNewSearchFolder);
		if (source != personalToolbarFolder.get())	cmdArray->AppendElement(kNC_BookmarkCommand_SetPersonalToolbarFolder);
	}

	// always append a separator last (due to aggregation of commands from multiple datasources)
	cmdArray->AppendElement(kNC_BookmarkSeparator);

	nsISimpleEnumerator		*result = new nsArrayEnumerator(cmdArray);
	if (!result)
		return(NS_ERROR_OUT_OF_MEMORY);
	NS_ADDREF(result);
	*commands = result;
	return(NS_OK);
}



NS_IMETHODIMP
nsBookmarksService::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                         nsIRDFResource*   aCommand,
                                         nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                         PRBool* aResult)
{
	return(NS_ERROR_NOT_IMPLEMENTED);
}



nsresult
nsBookmarksService::getArgumentN(nsISupportsArray *arguments, nsIRDFResource *res,
				PRInt32 offset, nsIRDFNode **argValue)
{
	nsresult		rv;
	PRUint32		loop, numArguments;

	*argValue = nsnull;

	if (NS_FAILED(rv = arguments->Count(&numArguments)))	return(rv);

	// format is argument, value, argument, value, ... [you get the idea]
	// multiple arguments can be the same, by the way, thus the "offset"
	for (loop = 0; loop < numArguments; loop += 2)
	{
		nsCOMPtr<nsISupports>	aSource = arguments->ElementAt(loop);
		if (!aSource)	return(NS_ERROR_NULL_POINTER);
		nsCOMPtr<nsIRDFResource>	src = do_QueryInterface(aSource);
		if (!src)	return(NS_ERROR_NO_INTERFACE);
		
		if (src.get() == res)
		{
			if (offset > 0)
			{
				--offset;
				continue;
			}

			nsCOMPtr<nsISupports>	aValue = arguments->ElementAt(loop + 1);
			if (!aSource)	return(NS_ERROR_NULL_POINTER);
			nsCOMPtr<nsIRDFNode>	val = do_QueryInterface(aValue);
			if (!val)	return(NS_ERROR_NO_INTERFACE);

			*argValue = val;
			NS_ADDREF(*argValue);
			return(NS_OK);
		}
	}
	return(NS_ERROR_INVALID_ARG);
}



nsresult
nsBookmarksService::insertBookmarkItem(nsIRDFResource *src, nsISupportsArray *aArguments, PRInt32 parentArgIndex, nsIRDFResource *objType)
{
	nsresult			rv;
	PRInt32				srcIndex = 0;
	nsCOMPtr<nsIRDFResource>	argParent;

	if (src == kNC_BookmarksRoot)
	{
		argParent = src;
	}
	else
	{
		nsCOMPtr<nsIRDFNode>	aNode;
		if (NS_FAILED(rv = getArgumentN(aArguments, kNC_Parent,
				parentArgIndex, getter_AddRefs(aNode))))
			return(rv);
		argParent = do_QueryInterface(aNode);
		if (!argParent)	return(NS_ERROR_NO_INTERFACE);
	}
	nsCOMPtr<nsIRDFContainer>	container;
	if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRDFContainerCID, nsnull,
			NS_GET_IID(nsIRDFContainer), getter_AddRefs(container))))
		return(rv);
	if (NS_FAILED(rv = container->Init(mInner, argParent)))
		return(rv);

	if (src != kNC_BookmarksRoot)
	{
		if (NS_FAILED(rv = container->IndexOf(src, &srcIndex)))
			return(rv);
	}

	nsAutoString			newName;

	if ((objType == kNC_Bookmark) || (objType == kNC_Folder))
	{
		nsCOMPtr<nsIRDFNode>	nameNode;
		if (NS_SUCCEEDED(rv = getArgumentN(aArguments, kNC_Name, parentArgIndex, getter_AddRefs(nameNode))))
		{
			nsCOMPtr<nsIRDFLiteral>	nameLiteral = do_QueryInterface(nameNode);
			if (nameLiteral)
			{
				const	PRUnichar	*nameUni = nsnull;
				nameLiteral->GetValueConst(&nameUni);
				if (nameUni)
				{
					newName = nameUni;
				}
			}
		}
	}
	if (newName.Length() == 0)
	{
		// get a default name
		if (objType == kNC_Bookmark)	getLocaleString("NewBookmark", newName);
		else if (objType == kNC_Folder)	getLocaleString("NewFolder", newName);
	}

	nsCOMPtr<nsIRDFResource>	newElement;

	if (objType == kNC_Bookmark)
	{
		nsCOMPtr<nsIRDFNode>	bookmarkNode;
		if (NS_SUCCEEDED(rv = getArgumentN(aArguments, kNC_URL, parentArgIndex, getter_AddRefs(bookmarkNode))))
		{
			nsCOMPtr<nsIRDFLiteral>	bookmarkURLLiteral = do_QueryInterface(bookmarkNode);
			if (!bookmarkURLLiteral)	return(NS_ERROR_NO_INTERFACE);
			const	PRUnichar	*urlUni = nsnull;
			bookmarkURLLiteral->GetValueConst(&urlUni);
			if (urlUni)
			{
				rv = gRDF->GetUnicodeResource(urlUni, getter_AddRefs(newElement));
				if (NS_FAILED(rv))	return(rv);
			}
		}
	}

	if (!newElement)
	{
		if (NS_FAILED(rv = BookmarkParser::CreateAnonymousResource(&newElement)))
			return(rv);
	}

	if (objType == kNC_Folder)
	{
		if (NS_FAILED(rv = gRDFC->MakeSeq(mInner, newElement, nsnull)))
			return(rv);
	}

	if (newName.Length() > 0)
	{
		nsCOMPtr<nsIRDFLiteral>	nameLiteral;
		if (NS_FAILED(rv = gRDF->GetLiteral(newName.GetUnicode(), getter_AddRefs(nameLiteral))))
			return(rv);
		if (NS_FAILED(rv = mInner->Assert(newElement, kNC_Name, nameLiteral, PR_TRUE)))
			return(rv);
	}

	if (NS_FAILED(rv = mInner->Assert(newElement, kRDF_type, objType, PR_TRUE)))
		return(rv);

	// convert the current date/time from microseconds (PRTime) to seconds
	nsCOMPtr<nsIRDFDate>	dateLiteral;
	if (NS_FAILED(rv = gRDF->GetDateLiteral(PR_Now(), getter_AddRefs(dateLiteral))))
		return(rv);
	if (NS_FAILED(rv = mInner->Assert(newElement, kNC_BookmarkAddDate, dateLiteral, PR_TRUE)))
		return(rv);

	if (NS_FAILED(rv = container->InsertElementAt(newElement, ((srcIndex == 0) ? 1 : srcIndex), PR_TRUE)))
		return(rv);
	return(rv);
}



nsresult
nsBookmarksService::deleteBookmarkItem(nsIRDFResource *src, nsISupportsArray *aArguments, PRInt32 parentArgIndex, nsIRDFResource *objType)
{
	nsresult			rv;

	nsCOMPtr<nsIRDFNode>		aNode;
	if (NS_FAILED(rv = getArgumentN(aArguments, kNC_Parent,
			parentArgIndex, getter_AddRefs(aNode))))
		return(rv);
	nsCOMPtr<nsIRDFResource>	argParent = do_QueryInterface(aNode);
	if (!argParent)	return(NS_ERROR_NO_INTERFACE);

	// make sure its an object of the correct type (bookmark, folder, separator, ...)
	PRBool	isCorrectObjectType = PR_FALSE;
	if (NS_FAILED(rv = mInner->HasAssertion(src, kRDF_type,
			objType, PR_TRUE, &isCorrectObjectType)))
		return(rv);
	if (!isCorrectObjectType)	return(NS_ERROR_UNEXPECTED);

	nsCOMPtr<nsIRDFContainer>	container;
	if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRDFContainerCID, nsnull,
			NS_GET_IID(nsIRDFContainer), getter_AddRefs(container))))
		return(rv);
	if (NS_FAILED(rv = container->Init(mInner, argParent)))
		return(rv);

	if (NS_FAILED(rv = container->RemoveElement(src, PR_TRUE)))
		return(rv);

	return(rv);
}



nsresult
nsBookmarksService::setFolderHint(nsIRDFResource *newSource, nsIRDFResource *objType)
{
	nsresult			rv;
	nsCOMPtr<nsISimpleEnumerator>	srcList;
	if (NS_FAILED(rv = GetSources(kNC_FolderType, objType, PR_TRUE, getter_AddRefs(srcList))))
		return(rv);

	PRBool	hasMoreSrcs = PR_TRUE;
	while(NS_SUCCEEDED(rv = srcList->HasMoreElements(&hasMoreSrcs))
		&& (hasMoreSrcs == PR_TRUE))
	{
		nsCOMPtr<nsISupports>	aSrc;
		if (NS_FAILED(rv = srcList->GetNext(getter_AddRefs(aSrc))))
			break;
		nsCOMPtr<nsIRDFResource>	aSource = do_QueryInterface(aSrc);
		if (!aSource)	continue;

		if (NS_FAILED(rv = mInner->Unassert(aSource, kNC_FolderType, objType)))
			continue;
	}

	// if not setting a new Personal Toolbar Folder, just assert new type, and then done

	if (objType != kNC_PersonalToolbarFolder)
	{
		if (NS_SUCCEEDED(rv = mInner->Assert(newSource, kNC_FolderType, objType, PR_TRUE)))
		{
		}
		mDirty = PR_TRUE;
		return(rv);
	}

	// else if setting a new Personal Toolbar Folder, we need to work some magic!

	nsCOMPtr<nsIRDFResource>	newAnonURL;
	if (NS_FAILED(rv = BookmarkParser::CreateAnonymousResource(&newAnonURL)))
		return(rv);
	// Note: use our Change() method, not mInner->Change(), due to Bookmarks magical #URL handling
	rv = Change(kNC_PersonalToolbarFolder, kNC_URL, kNC_PersonalToolbarFolder, newAnonURL);


	// change newSource's URL to be kURINC_PersonalToolbarFolder
	const char	*newSourceURI = nsnull;
	if (NS_FAILED(rv = newSource->GetValueConst( &newSourceURI )))	return(rv);
	nsCOMPtr<nsIRDFLiteral>		newSourceLiteral;
	if (NS_FAILED(rv = gRDF->GetLiteral(NS_ConvertASCIItoUCS2(newSourceURI).GetUnicode(),
			getter_AddRefs(newSourceLiteral))))	return(rv);

	// Note: use our Change() method, not mInner->Change(), due to Bookmarks magical #URL handling
	if (NS_FAILED(rv = Change(newSource, kNC_URL, newSourceLiteral, kNC_PersonalToolbarFolder)))
		return(rv);

	if (NS_FAILED(rv = mInner->Assert(kNC_PersonalToolbarFolder, kNC_FolderType, objType, PR_TRUE)))
		return(rv);

	mDirty = PR_TRUE;
	Flush();

	return(rv);
}



nsresult
nsBookmarksService::getFolderViaHint(nsIRDFResource *objType, PRBool fallbackFlag, nsIRDFResource **folder)
{
	if (!folder)	return(NS_ERROR_UNEXPECTED);
	*folder = nsnull;
	if (!objType)	return(NS_ERROR_UNEXPECTED);

	nsresult			rv;
	nsCOMPtr<nsIRDFResource>	oldSource;
	if (NS_FAILED(rv = mInner->GetSource(kNC_FolderType, objType, PR_TRUE, getter_AddRefs(oldSource))))
		return(rv);

	if ((rv != NS_RDF_NO_VALUE) && (oldSource))
	{
		const	char		*uri = nsnull;
		oldSource->GetValueConst(&uri);
		if (uri)
		{
			PRBool	isBookmarkedFlag = PR_FALSE;
			if (NS_SUCCEEDED(rv = IsBookmarked(uri, &isBookmarkedFlag)) &&
				(isBookmarkedFlag == PR_TRUE))
			{
				*folder = oldSource;
			}
		}
	}

	// if we couldn't find a real "New Internet Search Folder", fallback to looking for
	// a "New Bookmark Folder", and if can't find that, then default to the bookmarks root
	if ((!(*folder)) && (fallbackFlag == PR_TRUE) && (objType == kNC_NewSearchFolder))
	{
		rv = getFolderViaHint(kNC_NewBookmarkFolder, fallbackFlag, folder);
	}

	if (!(*folder))
	{
		// fallback to some well-known defaults
		if (objType == kNC_NewBookmarkFolder || objType == kNC_NewSearchFolder)
		{
			*folder = kNC_BookmarksRoot;
		}
		else if (objType == kNC_PersonalToolbarFolder)
		{
			*folder = kNC_PersonalToolbarFolder;
		}
	}

	NS_IF_ADDREF(*folder);

	return(NS_OK);
}



nsresult
nsBookmarksService::importBookmarks(nsISupportsArray *aArguments)
{
	// look for #URL which is the "file:///" URL to import
	nsresult		rv;
	nsCOMPtr<nsIRDFNode>	aNode;
	if (NS_FAILED(rv = getArgumentN(aArguments, kNC_URL, 0, getter_AddRefs(aNode))))
		return(rv);
	nsCOMPtr<nsIRDFLiteral>		pathLiteral = do_QueryInterface(aNode);
	if (!pathLiteral)	return(NS_ERROR_NO_INTERFACE);

	const PRUnichar		*pathUni = nsnull;
	pathLiteral->GetValueConst(&pathUni);
	if (!pathUni)	return(NS_ERROR_NULL_POINTER);

	nsAutoString		fileName(pathUni);
	nsFileURL		fileURL(fileName);
	nsFileSpec		fileSpec(fileURL);
	if (!fileSpec.IsFile())	return(NS_ERROR_UNEXPECTED);

	// figure out where to add the imported bookmarks
	nsCOMPtr<nsIRDFResource>	newBookmarkFolder;
	if (NS_FAILED(rv = getFolderViaHint(kNC_NewBookmarkFolder, PR_TRUE,
			getter_AddRefs(newBookmarkFolder))))
		return(rv);

	// read 'em in
	BookmarkParser		parser;
	parser.Init(&fileSpec, mInner, nsAutoString());
	parser.Parse(newBookmarkFolder, kNC_Bookmark);

	return(NS_OK);
}



nsresult
nsBookmarksService::exportBookmarks(nsISupportsArray *aArguments)
{
	// look for #URL which is the "file:///" URL to export
	nsresult		rv;
	nsCOMPtr<nsIRDFNode>	aNode;
	if (NS_FAILED(rv = getArgumentN(aArguments, kNC_URL, 0, getter_AddRefs(aNode))))
		return(rv);
	nsCOMPtr<nsIRDFLiteral>		pathLiteral = do_QueryInterface(aNode);
	if (!pathLiteral)	return(NS_ERROR_NO_INTERFACE);

	const PRUnichar		*pathUni = nsnull;
	pathLiteral->GetValueConst(&pathUni);
	if (!pathUni)	return(NS_ERROR_NULL_POINTER);

	nsAutoString		fileName(pathUni);
	nsFileURL		fileURL(fileName);
	nsFileSpec		fileSpec(fileURL);

	// write 'em out
	rv = WriteBookmarks(&fileSpec, mInner, kNC_BookmarksRoot);

	return(rv);
}



NS_IMETHODIMP
nsBookmarksService::DoCommand(nsISupportsArray *aSources, nsIRDFResource *aCommand,
				nsISupportsArray *aArguments)
{
	nsresult		rv = NS_OK;
	PRInt32			loop;
	PRUint32		numSources;
	if (NS_FAILED(rv = aSources->Count(&numSources)))	return(rv);
	if (numSources < 1)
	{
		return(NS_ERROR_ILLEGAL_VALUE);
	}

	// Note: some commands only run once (instead of looping over selection);
	//       if that's the case, be sure to "break" (if success) so that "mDirty"
	//       is set (and "bookmarks.html" will be flushed out shortly afterwards)

	for (loop=((PRInt32)numSources)-1; loop>=0; loop--)
	{
		nsCOMPtr<nsISupports>	aSource = aSources->ElementAt(loop);
		if (!aSource)	return(NS_ERROR_NULL_POINTER);
		nsCOMPtr<nsIRDFResource>	src = do_QueryInterface(aSource);
		if (!src)	return(NS_ERROR_NO_INTERFACE);

		if (aCommand == kNC_BookmarkCommand_NewBookmark)
		{
			rv = insertBookmarkItem(src, aArguments, loop, kNC_Bookmark);
			if (NS_FAILED(rv))	return(rv);
			break;
		}
		else if (aCommand == kNC_BookmarkCommand_NewFolder)
		{
			rv = insertBookmarkItem(src, aArguments, loop, kNC_Folder);
			if (NS_FAILED(rv))	return(rv);
			break;
		}
		else if (aCommand == kNC_BookmarkCommand_NewSeparator)
		{
			rv = insertBookmarkItem(src, aArguments, loop, kNC_BookmarkSeparator);
			if (NS_FAILED(rv))	return(rv);
			break;
		}
		else if (aCommand == kNC_BookmarkCommand_DeleteBookmark)
		{
			if (NS_FAILED(rv = deleteBookmarkItem(src, aArguments,
					loop, kNC_Bookmark)))
				return(rv);
		}
		else if (aCommand == kNC_BookmarkCommand_DeleteBookmarkFolder)
		{
			if (NS_FAILED(rv = deleteBookmarkItem(src, aArguments,
					loop, kNC_Folder)))
				return(rv);
		}
		else if (aCommand == kNC_BookmarkCommand_DeleteBookmarkSeparator)
		{
			if (NS_FAILED(rv = deleteBookmarkItem(src, aArguments,
					loop, kNC_BookmarkSeparator)))
				return(rv);
		}
		else if (aCommand == kNC_BookmarkCommand_SetNewBookmarkFolder)
		{
			rv = setFolderHint(src, kNC_NewBookmarkFolder);
			if (NS_FAILED(rv))	return(rv);
			break;
		}
		else if (aCommand == kNC_BookmarkCommand_SetPersonalToolbarFolder)
		{
			rv = setFolderHint(src, kNC_PersonalToolbarFolder);
			if (NS_FAILED(rv))	return(rv);
			break;
		}
		else if (aCommand == kNC_BookmarkCommand_SetNewSearchFolder)
		{
			rv = setFolderHint(src, kNC_NewSearchFolder);
			if (NS_FAILED(rv))	return(rv);
			break;
		}
		else if (aCommand == kNC_BookmarkCommand_Import)
		{
			rv = importBookmarks(aArguments);
			if (NS_FAILED(rv))	return(rv);
			break;
		}
		else if (aCommand == kNC_BookmarkCommand_Export)
		{
			rv = exportBookmarks(aArguments);
			if (NS_FAILED(rv))	return(rv);
			break;
		}
	}

	mDirty = PR_TRUE;

	return(NS_OK);
}

////////////////////////////////////////////////////////////////////////
// nsIRDFRemoteDataSource

NS_IMETHODIMP
nsBookmarksService::GetLoaded(PRBool* _result)
{
    *_result = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::Init(const char* aURI)
{
	return NS_OK;
}



NS_IMETHODIMP
nsBookmarksService::Refresh(PRBool aBlocking)
{
	// XXX re-sync with the bookmarks file, if necessary.
	return NS_OK;
}



NS_IMETHODIMP
nsBookmarksService::Flush()
{
	nsresult	rv = NS_OK;

	if (mBookmarksAvailable == PR_TRUE)
	{
		nsFileSpec	bookmarksFile;
		rv = GetBookmarksFile(&bookmarksFile);

		// Oh well, couldn't get the bookmarks file. Guess there
		// aren't any bookmarks for us to write out.
		if (NS_FAILED(rv))	return NS_OK;

		rv = WriteBookmarks(&bookmarksFile, mInner, kNC_BookmarksRoot);
	}
	return(rv);
}



////////////////////////////////////////////////////////////////////////
// Implementation methods

nsresult
nsBookmarksService::GetBookmarksFile(nsFileSpec* aResult)
{
	nsresult rv;

	// Look for bookmarks.html in the current profile
	// directory. This is as convoluted as it seems because we
	// want to 1) not break viewer (which has no profiles), and 2)
	// still deal reasonably (in the short term) when no
	// bookmarks.html is installed in the profile directory.
	do {
		nsCOMPtr <nsIFileSpec> bookmarksFile;

		NS_WITH_SERVICE(nsIFileLocator, locator, kFileLocatorCID, &rv);
		if (NS_FAILED(rv)) break;

		rv = locator->GetFileLocation(nsSpecialFileSpec::App_BookmarksFile50, getter_AddRefs(bookmarksFile));
		if (NS_FAILED(rv)) break;

		rv = bookmarksFile->GetFileSpec(aResult);
		if (NS_FAILED(rv)) break;
	} while (0);

#ifdef DEBUG
	if (NS_FAILED(rv)) {
		*aResult = nsSpecialSystemDirectory(nsSpecialSystemDirectory::OS_CurrentProcessDirectory);
		*aResult += "chrome";
		*aResult += "bookmarks";
		*aResult += "content";
		*aResult += "default";
		*aResult += "bookmarks.html";
		rv = NS_OK;
	}
#endif

	return rv;
}



#ifdef	XP_MAC

nsresult
nsBookmarksService::ReadFavorites()
{
	mIEFavoritesAvailable = PR_TRUE;
			
#ifdef	DEBUG
	PRTime		now;
	Microseconds((UnsignedWide *)&now);
	printf("Start reading in IE Favorites.html\n");
#endif

	// look for and import any IE Favorites
	nsAutoString	ieTitle;
	getLocaleString("ImportedIEFavorites", ieTitle);

	nsSpecialSystemDirectory ieFavoritesFile(nsSpecialSystemDirectory::Mac_PreferencesDirectory);
	ieFavoritesFile += "Explorer";
	ieFavoritesFile += "Favorites.html";

	nsresult	rv;
	if (NS_SUCCEEDED(rv = gRDFC->MakeSeq(mInner, kNC_IEFavoritesRoot, nsnull)))
	{
		BookmarkParser parser;
		parser.Init(&ieFavoritesFile, mInner, nsAutoString());
		parser.Parse(kNC_IEFavoritesRoot, kNC_IEFavorite);
			
		nsCOMPtr<nsIRDFLiteral>	ieTitleLiteral;
		rv = gRDF->GetLiteral(ieTitle.GetUnicode(), getter_AddRefs(ieTitleLiteral));
		if (NS_SUCCEEDED(rv) && ieTitleLiteral)
		{
			rv = mInner->Assert(kNC_IEFavoritesRoot, kNC_Name, ieTitleLiteral, PR_TRUE);
		}
	}
#ifdef	DEBUG
	PRTime		now2;
	Microseconds((UnsignedWide *)&now2);
	PRUint64	loadTime64;
	LL_SUB(loadTime64, now2, now);
	PRUint32	loadTime32;
	LL_L2UI(loadTime32, loadTime64);
	printf("Finished reading in IE Favorites.html  (%u microseconds)\n", loadTime32);
#endif
	return(rv);
}

#endif



NS_IMETHODIMP
nsBookmarksService::ReadBookmarks()
{
	nsresult	rv;

	// the profile manager might call Readbookmarks() in certain circumstances
	// so we need to forget about any previous bookmarks
	mInner = nsnull;
	if (NS_FAILED(rv = nsComponentManager::CreateInstance(kRDFInMemoryDataSourceCID,
				nsnull, NS_GET_IID(nsIRDFDataSource), (void**) &mInner)))
		return(rv);

	rv = mInner->AddObserver(this);
	if (NS_FAILED(rv)) return rv;

	nsFileSpec	bookmarksFile;
	rv = GetBookmarksFile(&bookmarksFile);

	// Oh well, couldn't get the bookmarks file. Guess there
	// aren't any bookmarks to read in.
	if (NS_FAILED(rv))
	    return NS_OK;

	rv = gRDFC->MakeSeq(mInner, kNC_BookmarksRoot, nsnull);
	NS_ASSERTION(NS_SUCCEEDED(rv), "Unable to make NC:BookmarksRoot a sequence");
	if (NS_FAILED(rv)) return rv;

	// Make sure bookmark's root has the correct type
	rv = mInner->Assert(kNC_BookmarksRoot, kRDF_type, kNC_Folder, PR_TRUE);
	if (NS_FAILED(rv)) return rv;

	PRBool	foundIERoot = PR_FALSE;

#ifdef	DEBUG
	PRTime		now;
#ifdef	XP_MAC
	Microseconds((UnsignedWide *)&now);
#else
	now = PR_Now();
#endif
	printf("Start reading in bookmarks.html\n");
#endif

#ifdef	XP_WIN
	nsCOMPtr<nsIRDFResource>	ieFolder;
	char				*ieFavoritesURL = nsnull;
#endif
#ifdef	XP_BEOS
	nsCOMPtr<nsIRDFResource>	netPositiveFolder;
	char				*netPositiveURL = nsnull;
#endif

	{ // <-- scope the stream to get the open/close automatically.
		BookmarkParser parser;
		parser.Init(&bookmarksFile, mInner, mPersonalToolbarName);

#ifdef	XP_MAC
		parser.SetIEFavoritesRoot(kURINC_IEFavoritesRoot);
#endif

#ifdef	XP_WIN
		nsSpecialSystemDirectory	ieFavoritesFile(nsSpecialSystemDirectory::Win_Favorites);
		nsFileURL			ieFavoritesURLSpec(ieFavoritesFile);
		const char			*favoritesURL = ieFavoritesURLSpec.GetAsString();
		if (favoritesURL)
		{
			ieFavoritesURL = strdup(favoritesURL);
		}
		parser.SetIEFavoritesRoot(ieFavoritesURL);
#endif

#ifdef	XP_BEOS
		nsSpecialSystemDirectory	netPositiveFile(nsSpecialSystemDirectory::BeOS_SettingsDirectory);
		nsFileURL			netPositiveURLSpec(netPositiveFile);

		// XXX Currently hard-coded; does the BeOS have anything like a
		// system registry which we could use to get this instead?
		netPositiveURLSpec += "NetPositive/Bookmarks/";

		const char			*constNetPositiveURL = netPositiveURLSpec.GetAsString();
		if (constNetPositiveURL)
		{
			netPositiveURL = strdup(constNetPositiveURL);
		}
		parser.SetIEFavoritesRoot(netPositiveURL);
#endif

		parser.Parse(kNC_BookmarksRoot, kNC_Bookmark);
		mBookmarksAvailable = PR_TRUE;
		
		PRBool	foundPTFolder = PR_FALSE;
		parser.ParserFoundPersonalToolbarFolder(&foundPTFolder);
		// try to ensure that we end up with a personal toolbar folder
		if ((foundPTFolder == PR_FALSE) && (mPersonalToolbarName.Length() > 0))
		{
			nsCOMPtr<nsIRDFLiteral>	ptNameLiteral;
			if (NS_SUCCEEDED(rv = gRDF->GetLiteral(mPersonalToolbarName.GetUnicode(),
				getter_AddRefs(ptNameLiteral))))
			{
				nsCOMPtr<nsIRDFResource>	ptSource;
				if (NS_FAILED(rv = mInner->GetSource(kNC_Name, ptNameLiteral,
						PR_TRUE, getter_AddRefs(ptSource))))
					return(rv);
				if ((rv != NS_RDF_NO_VALUE) && (ptSource))
				{
					setFolderHint(ptSource, kNC_PersonalToolbarFolder);
				}
			}
		}

		parser.ParserFoundIEFavoritesRoot(&foundIERoot);
	} // <-- scope the stream to get the open/close automatically.
	
	// look for and import any IE Favorites
	nsAutoString	ieTitle;
	getLocaleString("ImportedIEFavorites", ieTitle);

#ifdef	XP_BEOS
	nsAutoString	netPositiveTitle;
	getLocaleString("ImportedNetPositiveBookmarks", ieTitle);
#endif

#ifdef	XP_MAC
	// if the IE Favorites root isn't somewhere in bookmarks.html, add it
	if (!foundIERoot)
	{
		nsCOMPtr<nsIRDFContainer> bookmarksRoot;
		rv = nsComponentManager::CreateInstance(kRDFContainerCID,
							nsnull,
							NS_GET_IID(nsIRDFContainer),
							getter_AddRefs(bookmarksRoot));
		if (NS_FAILED(rv)) return rv;

		rv = bookmarksRoot->Init(mInner, kNC_BookmarksRoot);
		if (NS_FAILED(rv)) return rv;

		rv = bookmarksRoot->AppendElement(kNC_IEFavoritesRoot);
		if (NS_FAILED(rv)) return rv;

		// make sure IE Favorites root folder has the proper type		
		rv = mInner->Assert(kNC_IEFavoritesRoot, kRDF_type, kNC_IEFavoriteFolder, PR_TRUE);
		if (NS_FAILED(rv)) return rv;
	}
#endif

#ifdef	XP_WIN
	rv = gRDF->GetResource(ieFavoritesURL, getter_AddRefs(ieFolder));
	if (NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIRDFLiteral>	ieTitleLiteral;
		rv = gRDF->GetLiteral(ieTitle.GetUnicode(), getter_AddRefs(ieTitleLiteral));
		if (NS_SUCCEEDED(rv) && ieTitleLiteral)
		{
			rv = mInner->Assert(ieFolder, kNC_Name, ieTitleLiteral, PR_TRUE);
		}

		// if the IE Favorites root isn't somewhere in bookmarks.html, add it
		if (!foundIERoot)
		{
			nsCOMPtr<nsIRDFContainer> container;
			rv = nsComponentManager::CreateInstance(kRDFContainerCID,
								nsnull,
								NS_GET_IID(nsIRDFContainer),
								getter_AddRefs(container));
			if (NS_FAILED(rv)) return rv;

			rv = container->Init(mInner, kNC_BookmarksRoot);
			if (NS_FAILED(rv)) return rv;

			rv = container->AppendElement(ieFolder);
			if (NS_FAILED(rv)) return rv;
		}
	}
	if (ieFavoritesURL)
	{
		free(ieFavoritesURL);
		ieFavoritesURL = nsnull;
	}
#endif

#ifdef	XP_BEOS
	rv = gRDF->GetResource(netPositiveURL, getter_AddRefs(netPositiveFolder));
	if (NS_SUCCEEDED(rv))
	{
		nsCOMPtr<nsIRDFLiteral>	netPositiveTitleLiteral;
		rv = gRDF->GetLiteral(netPositiveTitle.GetUnicode(), getter_AddRefs(netPositiveTitleLiteral));
		if (NS_SUCCEEDED(rv) && netPositiveTitleLiteral)
		{
			rv = mInner->Assert(netPositiveFolder, kNC_Name, netPositiveTitleLiteral, PR_TRUE);
		}

		// if the Favorites root isn't somewhere in bookmarks.html, add it
		if (!foundIERoot)
		{
			nsCOMPtr<nsIRDFContainer> container;
			rv = nsComponentManager::CreateInstance(kRDFContainerCID,
								nsnull,
								NS_GET_IID(nsIRDFContainer),
								getter_AddRefs(container));
			if (NS_FAILED(rv)) return rv;

			rv = container->Init(mInner, kNC_BookmarksRoot);
			if (NS_FAILED(rv)) return rv;

			rv = container->AppendElement(netPositiveFolder);
			if (NS_FAILED(rv)) return rv;
		}
	}
	if (netPositiveURL)
	{
		free(netPositiveURL);
		netPositiveURL = nsnull;
	}
#endif

#ifdef	DEBUG
	PRTime		now2;
#ifdef	XP_MAC
	Microseconds((UnsignedWide *)&now2);
#else
	now2 = PR_Now();
#endif
	PRUint64	loadTime64;
	LL_SUB(loadTime64, now2, now);
	PRUint32	loadTime32;
	LL_L2UI(loadTime32, loadTime64);
	printf("Finished reading in bookmarks.html  (%u microseconds)\n", loadTime32);
#endif

	return(NS_OK);
}



nsresult
nsBookmarksService::WriteBookmarks(nsFileSpec *bookmarksFile, nsIRDFDataSource *ds,
				   nsIRDFResource *root)
{
	if (!bookmarksFile)	return(NS_ERROR_NULL_POINTER);
	if (!ds)		return(NS_ERROR_NULL_POINTER);
	if (!root)		return(NS_ERROR_NULL_POINTER);

	nsresult			rv;
	nsCOMPtr<nsISupportsArray>	parentArray;
	if (NS_FAILED(rv = NS_NewISupportsArray(getter_AddRefs(parentArray))))
		return(rv);

	rv = NS_ERROR_FAILURE;
	nsOutputFileStream	strm(*bookmarksFile);
	if (strm.is_open())
	{
		strm << "<!DOCTYPE NETSCAPE-Bookmark-file-1>\n";
		strm << "<!-- This is an automatically generated file.\n";
		strm << "It will be read and overwritten.\n";
		strm << "Do Not Edit! -->\n";

		// Note: we write out bookmarks in UTF-8
		strm << "<META HTTP-EQUIV=\"Content-Type\" CONTENT=\"text/html; charset=UTF-8\">\n";

		strm << "<TITLE>Bookmarks</TITLE>\n";
		strm << "<H1>Bookmarks</H1>\n\n";

		rv = WriteBookmarksContainer(ds, strm, root, 0, parentArray);
		mDirty = PR_FALSE;
	}
	return(rv);
}



nsresult
nsBookmarksService::WriteBookmarksContainer(nsIRDFDataSource *ds, nsOutputFileStream strm,
			nsIRDFResource *parent, PRInt32 level, nsISupportsArray *parentArray)
{
	nsresult	rv = NS_OK;

	nsCOMPtr<nsIRDFContainer> container;
	rv = nsComponentManager::CreateInstance(kRDFContainerCID, nsnull,
		NS_GET_IID(nsIRDFContainer), getter_AddRefs(container));
	if (NS_FAILED(rv)) return rv;

	nsAutoString	indentationString;
	  // STRING USE WARNING: converting in a loop.  Probably not a good idea
	for (PRInt32 loop=0; loop<level; loop++)	indentationString.AppendWithConversion("    ");
	char		*indentation = indentationString.ToNewCString();
	if (nsnull == indentation)	return(NS_ERROR_OUT_OF_MEMORY);

	strm << indentation;
	strm << "<DL><p>\n";

	rv = container->Init(ds, parent);
	if (NS_SUCCEEDED(rv) && (parentArray->IndexOf(parent) < 0))
	{
		// Note: once we've added something into the parentArray, don't "return" out
		//       of this function without removing it from the parentArray!
		parentArray->InsertElementAt(parent, 0);

		nsCOMPtr<nsISimpleEnumerator>	children;
		if (NS_SUCCEEDED(rv = container->GetElements(getter_AddRefs(children))))
		{
			PRBool	more = PR_TRUE;
			while (more == PR_TRUE)
			{
				if (NS_FAILED(rv = children->HasMoreElements(&more)))	break;
				if (more != PR_TRUE)	break;

				nsCOMPtr<nsISupports>	iSupports;					
				if (NS_FAILED(rv = children->GetNext(getter_AddRefs(iSupports))))	break;

				nsCOMPtr<nsIRDFResource>	child = do_QueryInterface(iSupports);
				if (!child)	break;

				PRBool	isContainer = PR_FALSE;
				if (child.get() != kNC_IEFavoritesRoot)
				{
					rv = gRDFC->IsContainer(ds, child, &isContainer);
					if (NS_FAILED(rv)) break;
				}

				nsCOMPtr<nsIRDFNode>	nameNode;
				nsAutoString		nameString;
				char			*name = nsnull;
				rv = ds->GetTarget(child, kNC_Name, PR_TRUE, getter_AddRefs(nameNode));
				if (NS_SUCCEEDED(rv) && nameNode)
				{
					nsCOMPtr<nsIRDFLiteral>	nameLiteral = do_QueryInterface(nameNode);
					if (nameLiteral)
					{
						const PRUnichar	*title = nsnull;
						if (NS_SUCCEEDED(rv = nameLiteral->GetValueConst(&title)))
						{
							nameString = title;
							name = nameString.ToNewUTF8String();
						}
					}
				}

				strm << indentation;
				strm << "    ";
				if (isContainer == PR_TRUE)
				{
					strm << "<DT><H3";
					// output ADD_DATE
					WriteBookmarkProperties(ds, strm, child, kNC_BookmarkAddDate, kAddDateEquals, PR_FALSE);

					// output LAST_MODIFIED
					WriteBookmarkProperties(ds, strm, child, kWEB_LastModifiedDate, kLastModifiedEquals, PR_FALSE);

					// output various special folder hints
					PRBool	hasType = PR_FALSE;
					if (NS_SUCCEEDED(rv = mInner->HasAssertion(child, kNC_FolderType, kNC_NewBookmarkFolder,
						PR_TRUE, &hasType)) && (hasType == PR_TRUE))
					{
						strm << " " << kNewBookmarkFolderEquals << "true\"";
					}
					if (NS_SUCCEEDED(rv = mInner->HasAssertion(child, kNC_FolderType, kNC_NewSearchFolder,
						PR_TRUE, &hasType)) && (hasType == PR_TRUE))
					{
						strm << " " << kNewSearchFolderEquals << "true\"";
					}
					if (NS_SUCCEEDED(rv = mInner->HasAssertion(child, kNC_FolderType, kNC_PersonalToolbarFolder,
						PR_TRUE, &hasType)) && (hasType == PR_TRUE))
					{
						strm << " " << kPersonalToolbarFolderEquals << "true\"";
					}

					// output ID
					const char	*id = nsnull;
					rv = child->GetValueConst(&id);
					if (NS_SUCCEEDED(rv) && (id))
					{
						strm << " " << kIDEquals << (const char *) id << "\"";
					}

					strm << ">";

					// output title
					if (name)	strm << name;
					strm << "</H3>\n";

					// output description (if one exists)
					WriteBookmarkProperties(ds, strm, child, kNC_Description, kOpenDD, PR_TRUE);

					rv = WriteBookmarksContainer(ds, strm, child, level+1, parentArray);
				}
				else
				{
					const char	*url = nsnull;
					if (NS_SUCCEEDED(rv = child->GetValueConst(&url)) && (url))
					{
						nsAutoString	uri; uri.AssignWithConversion(url);

						PRBool		isBookmarkSeparator = PR_FALSE;
						if (NS_SUCCEEDED(mInner->HasAssertion(child, kRDF_type,
							kNC_BookmarkSeparator, PR_TRUE, &isBookmarkSeparator)) &&
							(isBookmarkSeparator == PR_TRUE) )
						{
							// its a separator
							strm << "<HR>\n";
						}
						else
						{
							// output URL
							strm << "<DT><A HREF=\"";

							// Now do properly replace %22's; this is particularly important for javascript: URLs
							static const char kEscape22[] = "%22";
							PRInt32 offset;
							while ((offset = uri.FindChar(PRUnichar('\"'))) >= 0)
							{
								uri.Cut(offset, 1);
								uri.InsertWithConversion(kEscape22, offset);
							}
							char	*escapedID = uri.ToNewUTF8String();
							if (escapedID)
							{
								strm << (const char *) escapedID;
								nsCRT::free(escapedID);
								escapedID = nsnull;
							}

							strm << "\"";
								
							// output ADD_DATE
							WriteBookmarkProperties(ds, strm, child, kNC_BookmarkAddDate, kAddDateEquals, PR_FALSE);

							// output LAST_VISIT
							WriteBookmarkProperties(ds, strm, child, kWEB_LastVisitDate, kLastVisitEquals, PR_FALSE);

							// output LAST_MODIFIED
							WriteBookmarkProperties(ds, strm, child, kWEB_LastModifiedDate, kLastModifiedEquals, PR_FALSE);

							// output SHORTCUTURL
							WriteBookmarkProperties(ds, strm, child, kNC_ShortcutURL, kShortcutURLEquals, PR_FALSE);

							// output SCHEDULE
							WriteBookmarkProperties(ds, strm, child, kWEB_Schedule, kScheduleEquals, PR_FALSE);

							// output LAST_PING
							WriteBookmarkProperties(ds, strm, child, kWEB_LastPingDate, kLastPingEquals, PR_FALSE);

							// output PING_ETAG
							WriteBookmarkProperties(ds, strm, child, kWEB_LastPingETag, kPingETagEquals, PR_FALSE);

							// output PING_LAST_MODIFIED
							WriteBookmarkProperties(ds, strm, child, kWEB_LastPingModDate, kPingLastModEquals, PR_FALSE);

							// output PING_CONTENT_LEN
							WriteBookmarkProperties(ds, strm, child, kWEB_LastPingContentLen, kPingContentLenEquals, PR_FALSE);

							// output PING_STATUS
							WriteBookmarkProperties(ds, strm, child, kWEB_Status, kPingStatusEquals, PR_FALSE);

							strm << ">";
							// output title
							if (name)
							{
								// Note: we escape the title due to security issues;
								//       see bug # 13197 for details
								char *escapedAttrib = nsEscapeHTML(name);
								if (escapedAttrib)
								{
									strm << escapedAttrib;
									nsCRT::free(escapedAttrib);
									escapedAttrib = nsnull;
								}
							}
							strm << "</A>\n";
							
							// output description (if one exists)
							WriteBookmarkProperties(ds, strm, child, kNC_Description, kOpenDD, PR_TRUE);
						}
					}
				}

				if (nsnull != name)
				{
					nsCRT::free(name);
					name = nsnull;
				}
					
				if (NS_FAILED(rv))	break;
			}
		}

		// cleanup: remove current parent element from parentArray
		parentArray->RemoveElementAt(0);
	}

	strm << indentation;
	strm << "</DL><p>\n";

	nsCRT::free(indentation);
	return(rv);
}



/*
	Note: this routine is similiar, yet distinctly different from, nsRDFContentUtils::GetTextForNode
*/

nsresult
nsBookmarksService::GetTextForNode(nsIRDFNode* aNode, nsString& aResult)
{
    nsresult		rv;
    nsIRDFResource	*resource;
    nsIRDFLiteral	*literal;
    nsIRDFDate		*dateLiteral;
    nsIRDFInt		*intLiteral;

    if (! aNode) {
        aResult.Truncate();
        rv = NS_OK;
    }
    else if (NS_SUCCEEDED(rv = aNode->QueryInterface(NS_GET_IID(nsIRDFResource), (void**) &resource))) {
    	const char	*p = nsnull;
        if (NS_SUCCEEDED(rv = resource->GetValueConst( &p )) && (p)) {
            aResult.AssignWithConversion(p);
        }
        NS_RELEASE(resource);
    }
    else if (NS_SUCCEEDED(rv = aNode->QueryInterface(NS_GET_IID(nsIRDFDate), (void**) &dateLiteral))) {
	PRInt64		theDate, million;
        if (NS_SUCCEEDED(rv = dateLiteral->GetValue( &theDate ))) {
		LL_I2L(million, PR_USEC_PER_SEC);
		LL_DIV(theDate, theDate, million);			// convert from microseconds (PRTime) to seconds
		PRInt32		now32;
		LL_L2I(now32, theDate);
		aResult.Truncate();
        	aResult.AppendInt(now32, 10);
        }
        NS_RELEASE(dateLiteral);
    }
    else if (NS_SUCCEEDED(rv = aNode->QueryInterface(NS_GET_IID(nsIRDFInt), (void**) &intLiteral))) {
	PRInt32		theInt;
	aResult.Truncate();
        if (NS_SUCCEEDED(rv = intLiteral->GetValue( &theInt ))) {
        	aResult.AppendInt(theInt, 10);
        }
        NS_RELEASE(intLiteral);
    }
    else if (NS_SUCCEEDED(rv = aNode->QueryInterface(NS_GET_IID(nsIRDFLiteral), (void**) &literal))) {
	const PRUnichar		*p = nsnull;
        if (NS_SUCCEEDED(rv = literal->GetValueConst( &p )) && (p)) {
            aResult = p;
        }
        NS_RELEASE(literal);
    }
    else {
        NS_ERROR("not a resource or a literal");
        rv = NS_ERROR_UNEXPECTED;
    }

    return rv;
}



nsresult
nsBookmarksService::WriteBookmarkProperties(nsIRDFDataSource *ds, nsOutputFileStream strm,
	nsIRDFResource *child, nsIRDFResource *property, const char *htmlAttrib, PRBool isFirst)
{
	nsresult		rv;
	nsCOMPtr<nsIRDFNode>	node;
	if (NS_SUCCEEDED(rv = ds->GetTarget(child, property, PR_TRUE, getter_AddRefs(node)))
		&& (rv != NS_RDF_NO_VALUE))
	{
		nsAutoString	literalString;
		if (NS_SUCCEEDED(rv = GetTextForNode(node, literalString)))
		{
			char		*attribute = literalString.ToNewUTF8String();
			if (nsnull != attribute)
			{
				if (isFirst == PR_FALSE)
				{
					strm << " ";
				}
				if (property == kNC_Description)
				{
					if (literalString.Length() > 0)
					{
						char *escapedAttrib = nsEscapeHTML(attribute);
						if (escapedAttrib)
						{
							strm << htmlAttrib;
							strm << escapedAttrib;
							strm << "\n";

							nsCRT::free(escapedAttrib);
							escapedAttrib = nsnull;
						}
					}
				}
				else
				{
					strm << htmlAttrib;
					strm << attribute;
					strm << "\"";
				}
				nsCRT::free(attribute);
				attribute = nsnull;
			}
		}
	}
	return(rv);
}



PRBool
nsBookmarksService::CanAccept(nsIRDFResource* aSource,
			      nsIRDFResource* aProperty,
			      nsIRDFNode* aTarget)
{
	// XXX This is really crippled, and needs to be stricter. We want
	// to exclude any property that isn't talking about a known
	// bookmark.
	nsresult	rv;
	PRBool		canAcceptFlag = PR_FALSE, isOrdinal;

	if (NS_SUCCEEDED(rv = gRDFC->IsOrdinalProperty(aProperty, &isOrdinal)))
	{
		if (isOrdinal == PR_TRUE)
		{
			canAcceptFlag = PR_TRUE;
		}
		else if ((aProperty == kNC_Description) ||
			 (aProperty == kNC_Name) ||
			 (aProperty == kNC_ShortcutURL) ||
			 (aProperty == kNC_URL) ||
			 (aProperty == kWEB_LastModifiedDate) ||
			 (aProperty == kWEB_LastVisitDate) ||
			 (aProperty == kNC_BookmarkAddDate) ||
			 (aProperty == kRDF_nextVal) ||
			 (aProperty == kWEB_Schedule))
		{
			canAcceptFlag = PR_TRUE;
		}
	}
	return(canAcceptFlag);
}


//----------------------------------------------------------------------
//
// nsIRDFObserver interface
//

NS_IMETHODIMP
nsBookmarksService::OnAssert(nsIRDFResource* aSource,
			     nsIRDFResource* aProperty,
			     nsIRDFNode* aTarget)
{
	if (mObservers) {
		nsresult rv;

		PRUint32 count;
		rv = mObservers->Count(&count);
		if (NS_FAILED(rv)) return rv;

		for (PRInt32 i = 0; i < PRInt32(count); ++i) {
			nsIRDFObserver* obs =
				NS_REINTERPRET_CAST(nsIRDFObserver*, mObservers->ElementAt(i));

			(void) obs->OnAssert(aSource, aProperty, aTarget);
			NS_RELEASE(obs);
		}
	}

	return NS_OK;
}


NS_IMETHODIMP
nsBookmarksService::OnUnassert(nsIRDFResource* aSource,
			       nsIRDFResource* aProperty,
			       nsIRDFNode* aTarget)
{
	if (mObservers) {
		nsresult rv;

		PRUint32 count;
		rv = mObservers->Count(&count);
		if (NS_FAILED(rv)) return rv;

		for (PRInt32 i = 0; i < PRInt32(count); ++i) {
			nsIRDFObserver* obs =
				NS_REINTERPRET_CAST(nsIRDFObserver*, mObservers->ElementAt(i));

			(void) obs->OnUnassert(aSource, aProperty, aTarget);
			NS_RELEASE(obs);
		}
	}

	return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::OnChange(nsIRDFResource* aSource,
			     nsIRDFResource* aProperty,
			     nsIRDFNode* aOldTarget,
			     nsIRDFNode* aNewTarget)
{
	if (mObservers) {
		nsresult rv;

		PRUint32 count;
		rv = mObservers->Count(&count);
		if (NS_FAILED(rv)) return rv;

		for (PRInt32 i = 0; i < PRInt32(count); ++i) {
			nsIRDFObserver* obs =
				NS_REINTERPRET_CAST(nsIRDFObserver*, mObservers->ElementAt(i));

			(void) obs->OnChange(aSource, aProperty, aOldTarget, aNewTarget);
			NS_RELEASE(obs);
		}
	}

	return NS_OK;
}

NS_IMETHODIMP
nsBookmarksService::OnMove(nsIRDFResource* aOldSource,
			   nsIRDFResource* aNewSource,
			   nsIRDFResource* aProperty,
			   nsIRDFNode* aTarget)
{
	if (mObservers) {
		nsresult rv;

		PRUint32 count;
		rv = mObservers->Count(&count);
		if (NS_FAILED(rv)) return rv;

		for (PRInt32 i = 0; i < PRInt32(count); ++i) {
			nsIRDFObserver* obs =
				NS_REINTERPRET_CAST(nsIRDFObserver*, mObservers->ElementAt(i));

			(void) obs->OnMove(aOldSource, aNewSource, aProperty, aTarget);
			NS_RELEASE(obs);
		}
	}

	return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// Module implementation and export

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsBookmarksService, Init)

// The list of components we register
static nsModuleComponentInfo components[] = {
    { "Bookmarks", NS_BOOKMARKS_SERVICE_CID, NS_BOOKMARKS_SERVICE_PROGID,
      nsBookmarksServiceConstructor },
    { "Bookmarks", NS_BOOKMARKS_SERVICE_CID, NS_BOOKMARKS_DATASOURCE_PROGID,
      nsBookmarksServiceConstructor },
};

NS_IMPL_NSGETMODULE("nsBookmarkModule", components)
