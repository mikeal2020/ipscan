// Scanner.cpp: implementation of the CScanner class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "ipscan.h"
#include "Scanner.h"
#include "ScanUtilsInternal.h"
#include "IpscanDlg.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif


UINT g_nThreadCount = 0;
HANDLE g_hThreads[10000];
CDialog *g_dlg;
CIpscanDlg *g_d; 
CScanner *g_scanner;
COptions *g_options;

//////////////////////////////////////////////////////////////////////
// Built-in plugins (scanner columns)
//////////////////////////////////////////////////////////////////////

#define DEFAULT_LOADED_COLUMN_COUNT	3

TScannerColumn g_BuiltInScannerColumns[] = 
{
	// IP is always 0!!
	{
		/*pScanFunction*/ &ScanIntDoDummy,
		/*pInfoFunction*/ &ScanIntInfoDummy,
		/*pInitFunction*/ NULL,
		/*pFinaFunction*/ NULL
	},
	// Ping is always 1!!
	{
		/*pScanFunction*/ &ScanIntDoPing, 
		/*pInfoFunction*/ &ScanIntInfoPing,
		/*pInitFunction*/ &ScanIntInitPing,
		/*pFinaFunction*/ NULL
	},
	// Hostname
	{
		/*pScanFunction*/ &ScanIntDoHostname,
		/*pInfoFunction*/ &ScanIntInfoHostname,
		/*pInitFunction*/ NULL,
		/*pFinaFunction*/ NULL
	},
	// NetBIOS Computer Name
	{
		/*pScanFunction*/ &ScanIntDoNetBIOSComputerName,
		/*pInfoFunction*/ &ScanIntInfoNetBIOSComputerName,
		/*pInitFunction*/ &ScanIntInitNetBIOS,
		/*pFinaFunction*/ &ScanIntFinalizeNetBIOS
	},
	// NetBIOS Group Name
	{
		/*pScanFunction*/ &ScanIntDoNetBIOSGroupName,
		/*pInfoFunction*/ &ScanIntInfoNetBIOSGroupName,
		/*pInitFunction*/ &ScanIntInitNetBIOS,
		/*pFinaFunction*/ &ScanIntFinalizeNetBIOS
	},
	// NetBIOS User Name
	{
		/*pScanFunction*/ &ScanIntDoNetBIOSUserName,
		/*pInfoFunction*/ &ScanIntInfoNetBIOSUserName,
		/*pInitFunction*/ &ScanIntInitNetBIOS,
		/*pFinaFunction*/ &ScanIntFinalizeNetBIOS
	},
	// Mac Address
	{
		/*pScanFunction*/ &ScanIntDoNetBIOSMacAddress,
		/*pInfoFunction*/ &ScanIntInfoNetBIOSMacAddress,
		/*pInitFunction*/ &ScanIntInitNetBIOS,
		/*pFinaFunction*/ &ScanIntFinalizeNetBIOS
	}
};


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CScanner::CScanner()
{
	m_app = AfxGetApp();

	loadAllPossibleColumns();	

	loadSelectedColumns();		

}

void CScanner::loadAllPossibleColumns()
{
	m_nAllColumns = sizeof(g_BuiltInScannerColumns) / sizeof(TScannerColumn);	
	
	m_AllColumns.SetSize(m_nAllColumns + 10, 10);	
		
	// Load all possible columns
	for (int i=0; i < m_nAllColumns; i++)
	{
		m_AllColumns[i] = g_BuiltInScannerColumns[i];		
	}

	// TODO: plugin loading should be here somewhere

	TInfoStruct infoStruct;

	m_AllColumns[0].pszColumnName = new CString("IP");
	
	for (i=1; i < m_nAllColumns; i++)
	{
		m_AllColumns[i].pInfoFunction(&infoStruct);		
		
		m_AllColumns[i].pszColumnName = new CString(infoStruct.szColumnName);		
	}
}

void CScanner::loadSelectedColumns()
{
	// Load selected columns from Registry
	CString szColumns = m_app->GetProfileString("", "Columns", "");

	// Add the following columns - they always present
	m_nColumns = 0;	

	for (int i=0; i < CL_STATIC_COUNT; i++)
	{
		m_Columns[m_nColumns] = i;
		m_nColumns++;
	}

	// Parse the string	
	szColumns += " ";	// add a non-digit to the end to make parsing easier
	char szCurCol[6];
	int nCurColLen = 0;	
	
	for (i=0; i < szColumns.GetLength(); i++)
	{
		char chCur = szColumns.GetAt(i);
		
		if (chCur >= '0' && chCur <= '9')
		{			
			szCurCol[nCurColLen] = chCur;
			nCurColLen++;
		}
		else
		{
			if (nCurColLen == 0)
				continue;	// skip illegal character

			szCurCol[nCurColLen] = 0;			
			nCurColLen = 0;

			int nCurColumn = atoi(szCurCol);

			if (nCurColumn < m_nAllColumns && nCurColumn >= CL_STATIC_COUNT) // Add only if there is a column with this index
			{			
				m_Columns[m_nColumns] = nCurColumn;
				m_nColumns++;
			}
		}
	}

	if (m_nColumns <= CL_STATIC_COUNT)	// if no columns were loaded
	{
		m_nColumns = CL_STATIC_COUNT;	

		for (int i=CL_STATIC_COUNT; i < DEFAULT_LOADED_COLUMN_COUNT; i++)
		{
			m_Columns[m_nColumns] = i;
			m_nColumns++;
		}
	}
}

void CScanner::saveSelectedColumns()
{
	// Save selected columns to registry
	CString szColumns, szTmp;

	for (int i=CL_STATIC_COUNT; i < m_nColumns; i++)
	{
		szTmp.Format("%d ", m_Columns[i]);
		szColumns += szTmp;
	}

	m_app->WriteProfileString("", "Columns", szColumns);
}

CScanner::~CScanner()
{
	for (int i = 0; i < m_nColumns; i++)
	{
		delete m_AllColumns[i].pszColumnName;
	}
}

int CScanner::getColumnCount()
{
	return m_nColumns;
}

int CScanner::getAllColumnsCount()
{
	return m_nAllColumns;
}

BOOL CScanner::getColumnName(int nIndex, CString &szColumnHeader)
{
	szColumnHeader = *m_AllColumns[m_Columns[nIndex]].pszColumnName;
	return TRUE;
}

BOOL CScanner::getAllColumnName(int nIndex, CString &szColumnHeader)
{
	szColumnHeader = *m_AllColumns[nIndex].pszColumnName;
	return TRUE;
}

int CScanner::getColumnWidth(int nIndex)
{
	CString szName;
	getColumnName(nIndex, szName);
	szName = "Col_" + szName;	
	return m_app->GetProfileInt("", szName, 100);
}

int CScanner::getColumnReference(int nItemIndex)
{
	return m_Columns[nItemIndex];
}


void CScanner::initListColumns(CScanListCtrl *pListCtrl)
{
	int nCol, nWidth;		

	pListCtrl->DeleteAllItems();

	int nCurrentColumnCount = pListCtrl->GetColumnCount();	
	for (nCol=0; nCol < nCurrentColumnCount; nCol++)
	{
		pListCtrl->DeleteColumn(0);	// Delete the 1st column nCurrentColumnCount times
	}		
	
	for (nCol=0; nCol < m_nColumns; nCol++) 
	{					
		nWidth = getColumnWidth(nCol);
		pListCtrl->InsertColumn(nCol, *m_AllColumns[m_Columns[nCol]].pszColumnName, LVCFMT_LEFT, nWidth, nCol);
	}

	pListCtrl->SetScanPorts();	// Add / remove last column with port scanning
}

void CScanner::initMenuWithColumns(CMenu *pMenu)
{
	for (int nCol=CL_STATIC_COUNT; nCol < m_nAllColumns; nCol++) 
	{							
		pMenu->InsertMenu(nCol-CL_STATIC_COUNT, MF_BYPOSITION, ID_MENU_SHOW_CMD_001 + nCol-CL_STATIC_COUNT, *m_AllColumns[nCol].pszColumnName);		
		pMenu->EnableMenuItem(nCol-CL_STATIC_COUNT, MF_BYPOSITION | MF_ENABLED);
	}
}

void CScanner::runInitFunction(int nIndex, BOOL bAllFunctions)
{
	if (!bAllFunctions)
		nIndex = m_Columns[nIndex];

	if (m_AllColumns[nIndex].pInitFunction != NULL)
			m_AllColumns[nIndex].pInitFunction();
}

void CScanner::runFinalizeFunction(int nIndex, BOOL bAllFunctions)
{
	if (!bAllFunctions)
		nIndex = m_Columns[nIndex];

	if (m_AllColumns[nIndex].pFinalizeFunction != NULL)
			m_AllColumns[nIndex].pFinalizeFunction();
}

BOOL CScanner::initScanning()
{
	for (int i=0; i < m_nColumns; i++)
	{
		runInitFunction(i);
	}

	m_nAliveHosts = 0;
	m_nOpenPorts = 0;

	return TRUE;
}

BOOL CScanner::finalizeScanning()
{	
#ifndef _DEBUG
	for (int i=0; i < m_nColumns; i++)
	{
		runFinalizeFunction(i);
	}
#endif	// _DEBUG

	return TRUE;
}

BOOL CScanner::doScanIP(DWORD nParam, BOOL bParameterIsIP)
{
	DWORD nItemIndex;
	DWORD nIP;
	CString szOpenPorts;			// Open ports string will be saved here
	BOOL bSomePortsOpen = FALSE;	// Will be TRUE if some ports are open

	// Known callers:
	// RescanIP() calls this function by item index, but
	// Thread is called by the IP address
	if (bParameterIsIP)
	{
		nIP = nParam;							// IP is passed as parameter
		nItemIndex = (DWORD)nIP - g_nStartIP;	// Item index is counted as sequential, but it may be updated further, if item is not yet inserted
		nIP = htonl(nIP);						// Convert an IP to Network Byte Order
	}
	else
	{
		nItemIndex = nParam;					// ItemIndex is passed as parameter
		nIP = g_d->m_list.GetNumericIP(nItemIndex);		// IP address is obtained from the list (knowing the item index)
	}

	char szTmp[512];	// Temporary string. Scanning functions will return data using it.

	// Ping it! (column number 1), Check if it is alive
	BOOL bAlive = m_AllColumns[CL_PING].pScanFunction(nIP, (char*) &szTmp, sizeof(szTmp));	
	
	if (bAlive)	// This If is needed to insert an item or finish scanning this IP, other processing will follow
	{
		if (bParameterIsIP && g_options->m_neDisplayOptions == DISPLAY_ALIVE)
		{
			// Insert an item if it is not RescanIP() who called us and scanning mode is DISPLAY_ALIVE
			in_addr structInAddr;
			structInAddr.S_un.S_addr = nIP;
			char *szIP = inet_ntoa(structInAddr);
			nItemIndex = g_d->m_list.InsertItem(g_d->m_list.GetItemCount(), szIP, 2);	// 2nd image - "?"
		}
	
		// Increment number of alive hosts
		m_nAliveHosts++;
	}
	else
	{
		if (g_options->m_neDisplayOptions != DISPLAY_ALL)
		{
			// If was chosen to display only alive IPs or only open ports, then we shouldn't do anything further
			return TRUE;
		}
	}

	if (g_options->m_bScanPorts && g_options->m_neDisplayOptions == DISPLAY_OPEN)
	{
		// If display only open ports, then scan ports prior to scanning other columns
		bSomePortsOpen = doScanPorts(nIP, szOpenPorts);

		if (bSomePortsOpen)	// This IF is needed only to insert an item or finish scanning this IP, other processing will follow
		{
			if (bParameterIsIP)
			{
				// Insert an item if it is not RescanIP() who called us and scanning mode is DISPLAY_OPENPORT
				in_addr structInAddr;
				structInAddr.S_un.S_addr = nIP;
				char *szIP = inet_ntoa(structInAddr);
				nItemIndex = g_d->m_list.InsertItem(g_d->m_list.GetItemCount(), szIP, 2);	// 2nd image - "?"
			}
		}
		else
		{
			// If was chosen to display only open ports, then we shouldn't do anything further
			return TRUE;
		}
	}

	// At this point, the item is inserted into the list in any case!!!
	// So we can update the item in the list

	if (bAlive)	// Update the item according to alive/dead status
	{
		// Change image to Alive
		g_d->m_list.SetItem(nItemIndex, CL_IP, LVIF_IMAGE, NULL, 0, 0, 0, 0);
	}
	else
	{
		// Change image to Dead
		g_d->m_list.SetItem(nItemIndex, CL_IP, LVIF_IMAGE, NULL, 1, 0, 0, 0);
	}

	// Set item text returned from pinging (Dead or X ms)
	g_d->m_list.SetItemText(nItemIndex, CL_PING, (char*) &szTmp);
	
	
	// Scan other columns if sscanning of dead hosts is enabled or host is Alive
	bool bScan = g_options->m_bScanHostIfDead || bAlive; 

	// Run other scans (besides ping and port scanning)
	for (int i=CL_STATIC_COUNT; i < m_nColumns; i++)
	{
		if (bScan)
		{
			if (m_AllColumns[m_Columns[i]].pInfoFunction != NULL)
			{
				szTmp[0] = 0;
				runScanFunction(nIP, i, (char*) &szTmp, sizeof(szTmp));				
				
				// Returned an empty string
				if (szTmp[0] == 0)
					strcpy((char*)&szTmp, "N/A");
			}
			else
			{
				strcpy((char*)&szTmp, "ERR!");
			}
		}
		else
		{
			// Dead host, not scanned
			strcpy((char*) &szTmp, "N/S");
		}
		
		// Update the list with scanned info
		g_d->m_list.SetItemText(nItemIndex, i, (char*) &szTmp);
	}

	if (g_options->m_bScanPorts)
	{	
		if (bScan)
		{
			// Scan ports if they weren't scanned above already above
			if (g_options->m_neDisplayOptions != DISPLAY_OPEN)
			{
				bSomePortsOpen = doScanPorts(nIP, szOpenPorts);
			}
			
			if (bSomePortsOpen)	// bSomePortsOpen may be set already above
			{
				// Increment open ports
				m_nOpenPorts++;
			}
			
			// Update the list with open/closed ports
			g_d->m_list.SetOpenPorts(nItemIndex, szOpenPorts, bSomePortsOpen);	// szOpenPorts may be initialized above already
		}
		else
		{
			// Update the list and say that ports were not scanned
			g_d->m_list.SetOpenPorts(nItemIndex, "N/S", FALSE);
		}
	}

	return TRUE;
}

BOOL CScanner::doScanPorts(DWORD nIP, CString &szResult)
{
	szResult = "";

	// Scan port
	tPortRange *aPorts = g_options->m_aParsedPorts;

	SOCKET hSocket; 
	
	CString szPort;

	// Set socket to Non-Blocking mode
	u_long nNonBlocking = 1;	
	fd_set fd_write, fd_error;
	
	timeval timeout;
	timeout.tv_sec = 0; timeout.tv_usec = g_options->m_nPortTimeout * 1000;
	
	for (int nCurPortIndex = 0; aPorts[nCurPortIndex].nStartPort != 0; nCurPortIndex++)
	{		
		for (int nPort = aPorts[nCurPortIndex].nStartPort; nPort <= aPorts[nCurPortIndex].nEndPort; nPort++)
		{			
			hSocket = socket(PF_INET, SOCK_STREAM, IPPROTO_IP);
			// TODO: maximum number of sockets in the system
			sockaddr_in sin;
			sin.sin_addr.S_un.S_addr = nIP;
			sin.sin_family = PF_INET;
			ioctlsocket(hSocket, FIONBIO, &nNonBlocking);

			sin.sin_port = htons(nPort);
			connect(hSocket, (sockaddr*)&sin, sizeof(sin));

			fd_write.fd_array[0] = hSocket; fd_write.fd_count = 1;			
			fd_error.fd_array[0] = hSocket; fd_error.fd_count = 1;
			if (select(0, 0, &fd_write, &fd_error, &timeout) > 0) 
			{
				if (fd_write.fd_count == 1)
				{
					// Connection successfull
					szPort.Format("%d", nPort);
					szResult += szPort + ",";				
				}
			}
			
			closesocket(hSocket);
		}
	}

	BOOL bResult;
		
	if (szResult.GetLength() > 0)	// TRUE if any ports were open
	{
		// Strip comma from the end
		szResult.Delete(szResult.GetLength()-1);
		bResult = TRUE;
	}
	else
	{
		szResult = "N/A";
		bResult = FALSE;
	}

	return bResult;
}

void CScanner::runScanFunction(DWORD nIP, int nIndex, char *szBuffer, int nBufferLength, BOOL bGlobal /*=FALSE*/)
{
	if (bGlobal)
		m_AllColumns[nIndex].pScanFunction(nIP, szBuffer, nBufferLength);
	else
		m_AllColumns[m_Columns[nIndex]].pScanFunction(nIP, szBuffer, nBufferLength);
}


////////////////////////////////////////////////////////////////////////
//////////////////////////// THREAD ////////////////////////////////////
////////////////////////////////////////////////////////////////////////


UINT ThreadProcCallback(LPVOID nIP)
{	
	return ScanningThread((DWORD)nIP, IP_IS_GIVEN);
}

UINT ThreadProcCallbackRescan(LPVOID nItemIndex)
{	
	return ScanningThread((DWORD)nItemIndex, INDEX_IS_GIVEN);
}

UINT ScanningThread(DWORD nParam, BOOL bParameterIsIP)
{
	// Initialize thread //////////////////////////////////////////////////////	

	CString szTmp;	
	
	g_nThreadCount++;
	
	// Put thread's handle into global array (and find it's index)
	HANDLE hTmp;
	DuplicateHandle(GetCurrentProcess(),GetCurrentThread(),GetCurrentProcess(),&hTmp,0,FALSE,DUPLICATE_SAME_ACCESS);

	int nIndex;

	for (nIndex=0; nIndex<=10000; nIndex++) 
	{
		if (g_hThreads[nIndex]==0) 
		{ 			
			g_hThreads[nIndex] = hTmp;
			break; 
		}
	}

	g_d = (CIpscanDlg *) g_dlg;	

	// Display current number of threads
	szTmp.Format("%d", g_nThreadCount);
	g_d->m_numthreads.SetWindowText(szTmp);

	// Process scan /////////////////////////////////////////////////////////////
	/////////////////////////////////////////////////////////////////////////////

	g_scanner->doScanIP(nParam, bParameterIsIP);
	
	/////////////////////////////////////////////////////////////////////////////
	// Shutdown thread //////////////////////////////////////////////////////////	

	// Remove thread's handle	
	if (g_nThreadCount >=0) 
	{
		szTmp.Format("%d",g_nThreadCount);		
		g_d->m_numthreads.SetWindowText(szTmp);
	}

	CloseHandle(g_hThreads[nIndex]);

	g_hThreads[nIndex]=0;

	g_nThreadCount--;

	// Display current number of threads
	szTmp.Format("%d", g_nThreadCount);
	g_d->m_numthreads.SetWindowText(szTmp);
	
	return 0;

}

////////////////////////////////////////////////////////////////////////
//////////////////////////// THREAD ////////////////////////////////////
////////////////////////////////////////////////////////////////////////
