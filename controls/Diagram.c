#include <windows.h>
#include <winscard.h>
#define _Grid2Pix(_sz) ((_sz)*_GridSize)
#define _Pix2Grid(_sz) ((_sz)/_GridSize)

//todo: make connectors part of file structure
//todo: make usage and list arrays pointers instead of duplication from file structure
//todo: properly remove connections from delete items
//todo: string iteration should be done in vertical order (so sort connectors vertically?)

_const _MaxAllocationGap = 128;
_const _GridSize = 8;
_const _BlkMinWid = _Pix2Grid(80);
_const _BlkMinHei = _Pix2Grid(36);
_const _BlkMaxWid = 128; //_Pix2Grid(1024);
_const _BlkMaxHei = 128; //_Pix2Grid(1024);

typedef struct {
    union {
        uint32_t uConnector;
        struct {
            uint32_t uIndexHint   :12; //max 5096 objects per file
            uint32_t uUniqueID    :12; //Unique ID of source connector
            uint32_t uSourcePin   : 4;
            uint32_t uTargetPin   : 4; //non zero
        };
        struct {
            uint32_t uListID      :12; //ID of a list of connectors
            uint32_t uListSize    :12; //how many connectors are in the list
            uint32_t uReserved   : 4;
            uint32_t uOneConnector: 4; //zero
        };
    };
} ConnectorStruct;

typedef struct {
    //uint32_t iObjSize;        // Size of the object (do i need that?)
    uint16_t uUniqueID;         //+ 2= 2 // Unique ID of the object
    uint16_t iClassID;          //+ 2= 4 // ObjectClassID of the object
    int16_t  iY;                //+ 2= 6 // Y position of the object (in grid units)
    int16_t  iX;                //+ 2= 8 // X position of the object (in grid units)
    uint8_t  iW,iH;             //+ 2= 10 // Width and height of the object (grid units)
    uint8_t  bFlags, bResv;     //+ 2= 12 // Flags and reserved byte of the object
    ConnectorStruct tConnector; //+ 4= 16 // Connectors (if .uTargetPin is 0, but .uConnector is not 0)
    //                                       then .uIndexHint points to a list of connectors and
    //                                       .uUniqueID is how many connectors are there
    char zName[19], zZero;      //+20= 32 // Name (user) of the object and zero terminator
    char Content[0];            //       // type specific data follows...
} DiagramObjectStruct;
typedef struct {
    char zName[31], zZero;
    int iObjectCount;
    int iObjectMaxCount;
    DiagramObjectStruct** pObjects;
    //cache members
    int iViewX, iViewY; //in pixels
    int iSelectedIdx;
    char bUsedIDs[16384/8]; //bitmap of used IDs (1 bit per ID)
} DiagramFileStruct;

DiagramFileStruct* g_ProjectFiles[256]; int g_ProjectFileCount = 0;

typedef enum { //Diagram custom messages
    DIM_BASE = WM_USER,
    DIM_INSERT,              //Insert a new object in current diagram
    DIM_REMOVE,              //Remove the selected object from current diagram
    DIM_SELECT,              //Change the view to a new diagram
    DIM_GENERATE,            //Generate code for all diagrams
    DIM_TESTFILE,            //Generate a test file module
    DIM_SETCLASS,            //Set the class to add when inserting a new object
    /* PRIVATE ONES */
    DIM_CREATE_BUFFER,
} DiagramEnum;
typedef enum { //pin type
    pinNone    = 0,
    pinInput   = 1,
    pinOutput  = 2,
    pinExec    = 3,
    pinReserve = 4,
} PinType;

static DiagramObjectStruct** g_ptOrder = NULL;
static int g_iObjCount=0, g_iObjMaxCount=0;         //object counting / limit
#define aObject(_I) (*ptOrder[_I])
#define aObject_Content(_I,_T) (*((_T*)(ptOrder[_I]->Content)))

// retrieve the index and pin number from a connector structure, using a cached index if available
int IndexAndPinFromConnector( ConnectorStruct* tConnect , int* piPinNum ) {
    _const ptOrder = g_ptOrder;
    //if the cached index is correct then great return it and the pin number!
    //printf("cached index=%i uID=%i pin=%i\n",tConnect->uIndexHint,tConnect->uUniqueID,tConnect->uTargetPin);
    _with( aObject(tConnect->uIndexHint) ) {
        //printf("obj uID=%i -> ",w->uUniqueID);
        if ( w->uUniqueID == tConnect->uUniqueID ) {
            *piPinNum = tConnect->uTargetPin;
            //printf("cached at %i\n",tConnect->uIndexHint);
            return tConnect->uIndexHint;
        }
    } _endwith;
    //otherwise we search all elements to find its new position
    for (int i=0 ; i<g_iObjCount; i++ ) {
        _with( aObject(i) ) {
            if ( w->uUniqueID == tConnect->uUniqueID ) {
                tConnect->uIndexHint = i;
                *piPinNum = tConnect->uTargetPin;
                //printf("found at %i\n",i);
                return i;
            }
        } _endwith;
    }
    //and at least, if reaches here then it's not found
    //puts("not found");
    if (piPinNum) { *piPinNum = 0; }
    return -1;
}

// diagram window procedure, handles all messages for the diagram control
static CALLBACK LRESULT Diagram_WndProc ( HWND hwnd , UINT message, WPARAM wParam, LPARAM lParam ) {

    //#define _CTL(_ctlId) g_CTL[_ctlId].hwnd

    typedef enum { //resize sides
        rsNone   = 0,
        rsTop    = 1,
        rsBottom = 2,
        rsLeft   = 4,
        rsRight  = 8
    } ResizeState;
    typedef enum {
        dmtRedraw = 1,
    } DiagramTimers;
    static HBITMAP hBmBuffer;
    static HFONT hCtlFont,hSmallFont,hSmallFontB,hSymFont;
    static HDC hDcBuffer;
    static int iBufWid,iBufHei;
    static char bDrawn=1,bUpdateScroll=0,bHScroll=0,bVScroll=0;
    _const cBack=0xFFFFFF ; _const cGrid=0xEEEEEE ; _const cSelected=0x101010;
    _const cConnectingBad=0x0000AA ; _const cConnectingGood=0x00AA00;
    static HBRUSH hbBack , hbBackGrid , hbObject[256] = {0} ;
    static HPEN hpSelected, hpConnectingBad, hpConnectingGood;
    static char* IDC_CONNECT = MAKEINTRESOURCE(32631);

    #define SetUpdateAsync() if (bDrawn) { bDrawn=0 ; SetTimer( hwnd , dmtRedraw , 7 , NULL ); }
    #define SetUpdate() if (bDrawn) { bDrawn=0 ; SendMessage( hwnd , WM_TIMER , 0 , 0 ); SetTimer( hwnd , dmtRedraw , 1000/120 , NULL ); }

    static int iFreeSlotCount=0, iObjTotal=0;       //free slots in the ptObjects[] array
    static int iMaxX=0, iMaxY=0;                    //maximum position of any existing object (pixels)
    static int iViewX=0, iViewY=0;                  //scrolling offset (pixels)
    static char bUsedIDs[16384/8];                  //bitmap of used IDs (1 bit per ID)
    static int iMaxXIdx=-1 , iMaxYIdx=-1;           //indexes for the objects that have the maximum position (cache)
    static int iStartIdx=0, iEndIdx=-1;             //start/end indexes for drawn objects (cache)
    static int iSelectedIndex=-1,iHoverIndex=-1;    //current selected index // hovered index
    static int iMouseX=0,iMouseY=0;                 //last mouse position (pixels)
    static int iFontSize=0;
    static int iDragStartX,iDragStartY;              //position where drag started (if dragging) (pixels)
    static int iDragCancelX,iDragCancelY;            //original position of dragged element (if dragging) (pixels)
    static char bDragging=0, *pbCurCursor = NULL;    //0=no drag, 1=drag may start, 2=dragging // current cursor
    static RECT tResizeCancelRc;                     //original rectangle of the resized object
    static char bResizing=0, bLastSizeSide=0;        //0=no resize, n=resize side // current cursor
    static char bHoverPin=0, bPrevPin=0;             //last/previous pin state (for pin highlighting)
    static char bConnectingPin=0;                    //selected pin number when adding a connection
    static char bConnectingPinType=0;                //selected pin type when adding a connection
    static void* pbConnectingCursor = NULL;
    static DiagramFileStruct* pDiagram = NULL;
    static ConnectorStruct **g_pListConnectors = NULL;                     //array of lists for connectors
    static int g_iListConnectorsCount = 0, g_iListConnectorsMaxCount = 0;  //count max size of those lists
    static HWND hwndEdit = NULL; static DiagramObjectStruct** ppEditObj = NULL;
    static void* hwndOrgProc = NULL;
    static int iClassToAdd = -1;

    static const int cBlkBrd = 4;

    #include "../components/_basedecl.h"

    void GenerateFullCode( int iFileCount , DiagramFileStruct** pFile ) {
        #define emitf(...) iLen += sprintf( pCode+iLen , __VA_ARGS__ )
        _const cMaxBlockSize = 65536;
        char *pCodeBlocks[1024] = {malloc(cMaxBlockSize*2)}, *pCode = pCodeBlocks[0];
        int iLen = 0, iBlockCount = 0;
        if (pCode == NULL) return; pCode[0] = 0;
        emitf( "#include <stdio.h>\r\n" );
        emitf( "#include <stdlib.h>\r\n" );
        //emit one function per file/module
        for (int i=0 ; i < iFileCount ; i++ ) {
            ConsolePrintf("File #%i = '%s'\n", i, pFile[i]->zName);
            emitf( "int %s() {\r\n" , pFile[i]->zName );
            //list all objects in file
            for (int j=0 ; j < pFile[i]->iObjectCount ; j++ ) {
                _with(*pFile[i]->pObjects[j]) {
                    ConsolePrintf("  Object #%i = '%s'\n", j, w->zName); /*ClassInterfaceStruct*/
                    //tell the class handler to generate code for this object
                    iLen += g_ClassInterface[w->iClassID].pfHandlerProc( w->Content , g_ptOrder , CM_GenerateCode , cMaxBlockSize , (LPARAM)(pCode+iLen) );
                    if (iLen >= cMaxBlockSize) {
                        pCodeBlocks[iBlockCount++] = realloc(pCode, iLen);
                        pCodeBlocks[iBlockCount] = malloc(cMaxBlockSize*2);
                        pCode = pCodeBlocks[iBlockCount]; iLen = 0; pCode[0] = 0;
                    }
                } _endwith
            }
            emitf("}\r\n" );
        }
        //emit main function
        if (iFileCount) {
            emitf(
                "int main() {\r\n"
                "  %s();\r\n"
                "  getchar();\r\n"
                "  return 0;\r\n"
                "}\r\n"
                ,pFile[0]->zName
            );
        }

        ConsolePrintf( "%s" , "----------------------------------\r\n" );
        FILE* pFileOut = fopen("temp.c", "wb");
        for (int i=0 ; i <= iBlockCount ; i++ ) {
            ConsolePrintf( "%s" , pCodeBlocks[i] );
            fprintf(pFileOut, "%s", pCodeBlocks[i]);
            free(pCodeBlocks[i]); pCodeBlocks[i] = NULL;
        }
        fclose(pFileOut);
        iBlockCount = 0;

        #undef emitf
    }

    int ListConnectorsNew() {
        if (!g_pListConnectors) { g_iListConnectorsCount=1; }
        if (g_iListConnectorsCount >= g_iListConnectorsMaxCount) {
            g_iListConnectorsMaxCount += 8;
            g_pListConnectors = realloc( g_pListConnectors , sizeof(ConnectorStruct) * g_iListConnectorsMaxCount );
            for (int i=g_iListConnectorsMaxCount-8 ; i<g_iListConnectorsMaxCount ; i++ ) {
                g_pListConnectors[i]=0;
            }
        }
        g_iListConnectorsCount++;
        for (int i=1 ; i<g_iListConnectorsMaxCount ; i++ ) {
            if (!g_pListConnectors[i]) { return i; }
        }
        printf("It should never reach this point!!! at line %i\n",__LINE__);
        return 0;
    }

    // ------------- Diagram functions -------------

    int GetPinType( DiagramObjectStruct* pObject , int iPinNum ) {
        _with( *pObject ) {
            int iPinCnt = g_ClassInterface[w->iClassID].bInPins;
            if (iPinNum <= iPinCnt) { return pinInput; }
            iPinCnt += g_ClassInterface[w->iClassID].bOutPins;
            if (iPinNum <= iPinCnt) { return pinOutput; }
            iPinCnt += g_ClassInterface[w->iClassID].bExecPins;
            if (iPinNum <= iPinCnt) { return pinExec; }
        } _endwith;
        return pinNone;
    }
    int PinPositionFromObject( int iIndex, char bPinID , int* piX , int* piY ) {
        _const ptOrder = g_ptOrder;
        if (iIndex < 0 || iIndex >= g_iObjCount) { return 0; }
        if (bPinID < 1) { return 0; }
        _with( aObject(iIndex) ) {
            int iPinCnt = g_ClassInterface[w->iClassID].bInPins;
            if ( bPinID <= iPinCnt) { //input pin
                if (!piX) { return pinInput; }
                *piX = _Grid2Pix(w->iX)-iViewX+(_Grid2Pix(w->iW)*(bPinID))/(iPinCnt+1);
                *piY = _Grid2Pix(w->iY)-iViewY-(iFontSize*2)/5;
                return pinInput;
            }
            bPinID -= iPinCnt; iPinCnt = g_ClassInterface[w->iClassID].bOutPins;
            if ( bPinID <= iPinCnt) { //output pin
                if (!piX) { return pinOutput; }
                *piX = _Grid2Pix(w->iX)-iViewX+(_Grid2Pix(w->iW)*(bPinID))/(iPinCnt+1);
                *piY = _Grid2Pix(w->iY+w->iH)-iViewY+(iFontSize*2)/5;
                return pinOutput;
            }
            bPinID -= iPinCnt; iPinCnt = g_ClassInterface[w->iClassID].bExecPins;
            if ( bPinID <= iPinCnt) { //exec pin
                if (!piX) { return pinExec; }
                *piX = _Grid2Pix(w->iX+w->iW)-iViewX;
                *piY = _Grid2Pix(w->iY)-iViewY+(iFontSize/4)+(_Grid2Pix(w->iH)*(bPinID))/(iPinCnt+1);
                return pinExec;
            }
        } _endwith;
        return 0;
    }
    // source object and source pin to start connection
    void StartConnectingPin( int iSrcObject, int iSrcPin ) { //Initiate the drag to connect objects
        _const ptOrder = g_ptOrder;
        if (iSrcObject < 0 || iSrcObject >= g_iObjCount) { return; }
        if (!iSrcPin) { return; }
        iSelectedIndex = iSrcObject; bConnectingPin = iSrcPin;
        //printf("Selected pin %i\n",iSrcPin);
        if (!(bConnectingPinType = GetPinType( &aObject(iSrcObject) , iSrcPin ))) { return; }
    }
    // target object and target pin to check or add connection
    #define CancelConnectingPin() ActConnectingPin( -1 , 0 , 1 )
    #define CheckConnectingPin( _Obj , _Pin ) ActConnectingPin( _Obj , _Pin , 0 )
    #define AddConnectingPin( _Obj , _Pin ) ActConnectingPin( _Obj , _Pin , 1 )

    #define CheckInsertConnectionPin( _SO , _SP , _TO , _TP ) MayInsertConnectionPin( &aObject(_SO) , _SO , _SP , &aObject(_TO) , _TP , false )
    #define InsertConnectionPin( _SO , _SP , _TO , _TP ) MayInsertConnectionPin( &aObject(_SO) , _SO , _SP , &aObject(_TO) , _TP , true )
    int MayInsertConnectionPin( DiagramObjectStruct *pSrcObject , int iSrcHint , int iSrcPin , DiagramObjectStruct *pTgtObject , int iTgtPin , BOOL bAddPin /* = 0 */ ) {
        int iConnCnt = 0;
        _with( *pTgtObject ) {
            ConnectorStruct* pCon = &w->tConnector;
            if (pCon->uConnector) {
                iConnCnt = 1;  //if connector is not empty then there's 1 element
                if (!pCon->uOneConnector) { //but then... if uOneConnector is 0 there's more than 1 element
                    iConnCnt = pCon->uListSize; pCon = g_pListConnectors[pCon->uListID];
                }
            }

            _auto uuid = pSrcObject->uUniqueID;
            // check if srcObject + SrcPin already exists in connectors list
            for (int i=0 ; i<iConnCnt ; i++) {
                if (pCon[i].uUniqueID == uuid) {
                   if ((pCon[i].uSourcePin == iSrcPin) && (pCon[i].uTargetPin == iTgtPin)) {
                       if (bAddPin) { puts("Connection already exist..."); }
                       return -1;
                   }
                }
            }
            if (!bAddPin) { return iConnCnt; }

            //printf("Connectors: %i\n", iConnCnt);

            if (iConnCnt == 1) { //if there was 1 element then create a list
                int iNewListID = ListConnectorsNew();
                g_pListConnectors[iNewListID]=pCon=malloc( sizeof(ConnectorStruct)*2 ); //allocate a connector
                pCon[0].uConnector = w->tConnector.uConnector; //copy the existing connector to the new list
                w->tConnector.uListID = iNewListID; w->tConnector.uListSize = 2; w->tConnector.uOneConnector = 0;
                //printf("New List = (%i)%p",iNewListID,pCon);
            }            //now add the new connector to the list (resizing the list if at least 1 connector was there)
            if (iConnCnt > 1) {
                pCon=realloc( pCon , sizeof(ConnectorStruct)*(iConnCnt+1) );
                g_pListConnectors[w->tConnector.uListID]=pCon;
                w->tConnector.uListSize++;
            }
            pCon[iConnCnt].uUniqueID = uuid;
            pCon[iConnCnt].uIndexHint = iSrcHint;
            pCon[iConnCnt].uSourcePin = iSrcPin;
            pCon[iConnCnt].uTargetPin = iTgtPin; //this also sets .uOneConnector
        } _endwith
        return iConnCnt+1;
    }
    int  ActConnectingPin( int iTgtObject, int iTgtPin , BOOL bAddPin /* = 0 */ ) { //check,add or cancel a connection
        _const ptOrder = g_ptOrder;
        #define _CancelConnection() { if (bAddPin) { puts("Cancelled connecting pin"); bConnectingPin = 0; }; return 0; }
        //printf("Obj=%i Pin=%i Add=%i\n",g_iObject,iPin,bAddPin);
        //cancels the connection when iTgtPin is 0, while adding pin.
        if (!iTgtPin && bAddPin) {  _CancelConnection(); }

        //invalid/null object, or pin... make checks to fail (connecting to self is also invalid)
        _const iSrcObject = iSelectedIndex; _const iSrcPin = bConnectingPin; _const bSrcPinType = bConnectingPinType;
        if ((iTgtObject<0 || !iTgtPin) || ( iTgtObject==iSrcObject)) { _CancelConnection(); }
        _const bTgtPinType = PinPositionFromObject( iTgtObject , iTgtPin , 0 , 0 );
        if (bTgtPinType == bSrcPinType) { _CancelConnection(); }
        if (CheckInsertConnectionPin( iSrcObject , iSrcPin , iTgtObject , iTgtPin )<0) { _CancelConnection(); }
        if (!bAddPin) { return 1; } //just checking if a connection is possible

        //add the connection to the object...
        //should we add to both objects for cross check?
        //making the user responsible for proper source->target direction is bad i guess
        //so if we don't add to both, we need to add an "input pin" to an "output pin"
        //meaning source/target objects can be swapped? exec pins are callback to function not to pins?

        //printf("Adding connection from %i.%i to %i.%i\n", iSrcObject, iSrcPin, iTgtObject, iTgtPin);
        InsertConnectionPin( iSrcObject , iSrcPin , iTgtObject , iTgtPin );
        InsertConnectionPin( iTgtObject , iTgtPin , iSrcObject , iSrcPin );

        bConnectingPin = 0; return 1;
        #undef _CancelConnection
    }

    void ScrollUpdate( HWND hwnd , int nWid , int nHei ) {
        _const ptOrder = g_ptOrder;
        //if client are is not given... calculate
        if (nWid < 0) {
            RECT tRc ; GetClientRect( hwnd , &tRc );
            nWid = tRc.right ; nHei = tRc.bottom;
        }
        //if max size/indexes need to be found, do it now
        if (iMaxXIdx < 0) {
            iMaxX = iMaxY = -1;
            for (int i=0 ; i < g_iObjCount ; i++ ) {
                _with( aObject(i) ) {
                    if (_Grid2Pix(w->iX+w->iW) > iMaxX) { iMaxX = _Grid2Pix(w->iX+w->iW); iMaxXIdx=i; }
                    if (_Grid2Pix(w->iY+w->iH) > iMaxY) { iMaxY = _Grid2Pix(w->iY+w->iH); iMaxYIdx=i; }
                } _endwith;
            }
        }

        //update scrollbar max sizes , page , range
        SCROLLINFO tInfoH = { .cbSize = sizeof(SCROLLINFO) , .fMask = SIF_PAGE | SIF_RANGE | SIF_DISABLENOSCROLL , .nPage = nWid , .nMin = 0 , .nMax = (iMaxX >= nWid) ? iMaxX+nWid/2 : iMaxX };
        SetScrollInfo( hwnd , SB_HORZ , &tInfoH , true );
        SCROLLINFO tInfoV = { .cbSize = sizeof(SCROLLINFO) , .fMask = SIF_PAGE | SIF_RANGE | SIF_DISABLENOSCROLL , .nPage = nHei , .nMin = 0 , .nMax = (iMaxY >= nHei) ? iMaxY+nHei/2 : iMaxY };
        SetScrollInfo( hwnd , SB_VERT , &tInfoV , true );
        bUpdateScroll = 1;
    }
    void WindowDraw(void) {
        _const ptOrder = g_ptOrder;
        //check if there's previous items that are visible (caused by moving or scroll up)
        if (bDrawn==1) { return; }

        HDC hdc = hDcBuffer;
        RECT tRc = {0,0,iBufWid,iBufHei};

        iViewX = GetScrollPos( hwnd , SB_HORZ );
        int iTempViewY = GetScrollPos( hwnd , SB_VERT );
        iViewY = (iViewY*3+iTempViewY+2)/4;
        if ((abs(iViewY-iTempViewY) > iBufHei)) { iViewY = (iViewY+iTempViewY+1)/2; }
        if (iTempViewY < iViewY) { iViewY--; }
        if (iTempViewY > iViewY) { iViewY++; }

        SetBrushOrgEx( hdc , 1 , 4-(iViewY & 7) , NULL );
        SetBrushOrgEx( hdc , 4 , 4-(iViewY & 7) , NULL );
        FillRect( hdc , &tRc , hbBackGrid );
        SetBkMode( hdc , TRANSPARENT );
        if (GetFocus()==hwnd) { DrawFocusRect( hdc , &tRc ); }

        if (!g_ptOrder) { return; }
        //puts("drawing start");

        if (iStartIdx >= g_iObjCount) { iStartIdx = g_iObjCount-1; }
        if (iStartIdx < 0) { iStartIdx = 0; }
        if (iEndIdx >= g_iObjCount) { iEndIdx = g_iObjCount-1; }

        for ( ; iStartIdx>0 ; iStartIdx-- ) {
            _with( aObject(iStartIdx-1) ) {
                if ((_Grid2Pix(w->iY+w->iH)-iViewY) < 0) { break; }
            } _endwith;
        }

        int iIndex, iFontHeight, iFontWidth;
        SelectObject( hDcBuffer , hSymFont );
        {
            SIZE tFontSz;
            GetTextExtentPoint32( hDcBuffer , "AWI" , 1 , &tFontSz );
            iFontWidth = tFontSz.cx/3; iFontHeight = iFontSize = tFontSz.cy;
        }

        for ( iIndex=iStartIdx ; (iIndex < g_iObjCount) ; iIndex++) {
            if (iIndex < (g_iObjCount-1)) {
                _with( aObject(iIndex+1) ) {
                    //check/skip if item is invisible (caused by moving or scroll down)
                    if ((_Grid2Pix(w->iY+w->iH)-iViewY) < 0) { iStartIdx += 1 ; continue ; }
                } _endwith
            }
            _with( aObject(iIndex) ) {
                //clculate position Y and see if it's after the visible area (early stop)
                int iPosY = _Grid2Pix(w->iY)-iViewY, iPosX = _Grid2Pix(w->iX)-iViewX;
                if (iPosY >= iBufHei) { break; }
                //skip object if outside horizontal range
                if ( (iPosX+_Grid2Pix(w->iW)) < 0 || iPosX >= iBufWid ) { continue; }

                /* printf( "#%i uid=%i class='%s' name='%s'\n",
                    iIndex, w->uUniqueID, g_ClassInterface[w->iClassID].pzName, w->zName ); */

            } _endwith
        }

        for ( iIndex=iStartIdx ; (iIndex < g_iObjCount) ; iIndex++) {
            if (iIndex < (g_iObjCount-1)) {
                _with( aObject(iIndex+1) ) {
                    //check/skip if item is invisible (caused by moving or scroll down)
                    if ((_Grid2Pix(w->iY+w->iH)-iViewY) < 0) { iStartIdx += 1 ; continue ; }
                } _endwith
            }
            _with( aObject(iIndex) ) {
                //clculate position Y and see if it's after the visible area (early stop)
                int iPosY = _Grid2Pix(w->iY)-iViewY, iPosX = _Grid2Pix(w->iX)-iViewX;
                if (iPosY >= iBufHei) { break; }
                //skip object if outside horizontal range
                if ( (iPosX+_Grid2Pix(w->iW)) < 0 || iPosX >= iBufWid ) { continue; }

                //draw connections
                if (w->tConnector.uConnector) { //there's connections???
                    ConnectorStruct* ptList = &w->tConnector; int iConCnt = 1, iList=-1;
                    if (!w->tConnector.uOneConnector) { //it's a list instead of a single connection
                        iList = w->tConnector.uListID;
                        ptList = g_pListConnectors[iList]; iConCnt = w->tConnector.uListSize;

                    }
                    //printf("Index: %i , Connections = %i List=(%i)%p\n",iIndex,iConCnt,iList,ptList);
                    for (int iCon=0 ; iCon < iConCnt ; iCon++) {
                        _with (ptList[iCon]) {
                            int iTgtPin = 0, iTgtIdx = IndexAndPinFromConnector( ptList+iCon , &iTgtPin );
                            if (iTgtIdx < 0) { puts("Internal error drawing connector"); continue; }
                            //printf("Connection #%i > %i:%i to %i:%i\n",iCon,iIndex,w->uSourcePin,iTgtIdx,iTgtPin);
                            int iX,iY,iXX,iYY;
                            if (GetPinType( &aObject(iIndex) , w->uSourcePin ) < GetPinType( &aObject(iTgtIdx) , iTgtPin )) {
                                PinPositionFromObject( iIndex  , w->uSourcePin , &iXX , &iYY );
                                PinPositionFromObject( iTgtIdx , iTgtPin , &iX , &iY );
                            } else {
                                PinPositionFromObject( iIndex  , w->uSourcePin , &iX , &iY );
                                PinPositionFromObject( iTgtIdx , iTgtPin , &iXX , &iYY );
                            }
                            for (int iN=0; iN<3; iN++) {
                                const int iOX = (iN & 1), iOY = (iN/2);
                                const POINT atBezier[] = {
                                    {iOX+iX         , iOY+iY} ,
                                    {iOX+iX         , iOY+iYY} ,
                                    {iOX+(iX+iXX)/2 , iOY+(iY+iYY)/2} ,
                                    {iOX+iXX        , iOY+iYY}
                                };
                                PolyBezier( hdc , atBezier , 4 );
                            }
                        } _endwith;
                    }
                }

                //render object
                SelectObject( hdc , hbObject[ w->iClassID ] );
                RECT tObjRc = {iPosX,iPosY,iPosX+_Grid2Pix(w->iW),iPosY+_Grid2Pix(w->iH)};
                RoundRect( hdc , tObjRc.left , tObjRc.top , tObjRc.right , tObjRc.bottom , 16 , 16 );

                //border if selected (single select)
                if (iIndex == iSelectedIndex) {
                    _const hOldPen = SelectObject( hdc , hpSelected );
                    _const hOldBrush = SelectObject( hdc , GetStockObject( NULL_BRUSH ) );
                    RoundRect( hdc , tObjRc.left-0 , tObjRc.top-0 , tObjRc.right+0 , tObjRc.bottom+0 , 16 , 16 );
                    SelectObject( hdc , hOldPen ); SelectObject( hdc , hOldBrush );
                }

                int iWid=tObjRc.right-tObjRc.left, iHei=tObjRc.bottom-tObjRc.top;
                int iBorderUD = (iHei)/4; tObjRc.bottom -= 4;
                SelectObject( hDcBuffer , hSmallFont );
                DrawText( hdc , w->zName , -1 , &tObjRc , DT_SINGLELINE | DT_CENTER | DT_BOTTOM | DT_NOPREFIX );
                SelectObject( hDcBuffer , hSmallFontB );
                DrawText( hdc , g_ClassInterface[w->iClassID].pzName , -1 , &tObjRc , DT_SINGLELINE | DT_CENTER | DT_TOP | DT_NOPREFIX );

                SelectObject( hDcBuffer , hSymFont );
                //printf("raw=%02X , PinType=%d , PinNum=%d\n", (int)bHoverPin, (int)bHoverPin>>5, (int)bHoverPin&0x1F);
                int iCurPin=(iIndex==iHoverIndex?1:128);
                { // draw input pins
                    SetTextAlign( hDcBuffer , TA_CENTER |TA_BOTTOM );
                    int iPinCnt=g_ClassInterface[w->iClassID].bInPins, iPinSpace = (iWid)/(iPinCnt+1);
                    for (int i=iPinSpace+(iFontWidth/4) ; iPinCnt-- ; i += iPinSpace, iCurPin++) {
                        SetTextColor( hDcBuffer , (iCurPin==bHoverPin) ? RGB( 224 , 0 , 0 ) : RGB( 128 , 0 , 0 ) );
                        TextOut( hdc , tObjRc.left+i , tObjRc.top+iFontHeight/4 , "\x88" , 1 );
                    }
                }
                { // draw output pins
                    SetTextAlign( hDcBuffer , TA_CENTER |TA_TOP );
                    int iPinCnt=g_ClassInterface[w->iClassID].bOutPins, iPinSpace = (iWid)/(iPinCnt+1);
                    for (int i=iPinSpace ; iPinCnt-- ; i += iPinSpace, iCurPin++) {
                        SetTextColor( hDcBuffer , (iCurPin==bHoverPin) ? RGB( 0 , 192 , 0 ) : RGB( 0 , 128 , 0 ) );
                        TextOut( hdc , tObjRc.left+i , tObjRc.bottom , "\x98" , 1 ); //-iFontHeight/4
                    }
                }
                { // draw exec pins
                    SetTextAlign( hDcBuffer , TA_LEFT );
                    int iPinCnt=g_ClassInterface[w->iClassID].bExecPins, iPinSpace = (iHei)/(iPinCnt+1);
                    for (int i=iPinSpace-iFontHeight/2 ; iPinCnt-- ; i += iPinSpace, iCurPin++) {
                        SetTextColor( hDcBuffer , (iCurPin==bHoverPin) ? RGB( 0 , 0 , 255 ) : RGB( 0 , 0 , 128 ) );
                        TextOut( hdc , tObjRc.right-2 , tObjRc.top+i , "\xB2" , 1 ); //-iFontHeight/4
                    }
                }
                SetTextAlign( hDcBuffer , TA_LEFT );
                SetTextColor( hDcBuffer , RGB( 0 , 0 , 0 ) );

                //tell object to draw itself
                tObjRc.top    += (iBorderUD) ; tObjRc.bottom -= (iBorderUD-4);
                //if (iIndex==0) printf("(draw) content=%p\n", w->Content);
                g_ClassInterface[w->iClassID].pfHandlerProc( w->Content , NULL , WM_PAINT , 0 , (LPARAM)&tObjRc );

            } _endwith;
        }

        iEndIdx=(iIndex < g_iObjCount ? iIndex-1 : g_iObjCount-1);

        //draw dragging connection if any
        if (bConnectingPin) {
            //bConnectingPin have the PIN, iSelectedIndex have the object, iMouseX/iMouseY have the mouse position
            //tracing a path from the selected pin to the mouse position
            int iX,iY, iXX, iYY, iSrcType=0, iTgtType=0; pbConnectingCursor = IDC_NO;
            if (iSrcType = PinPositionFromObject( iSelectedIndex , bConnectingPin , &iX , &iY )) {
                iXX = iMouseX; iYY = iMouseY;
                _auto hpConnecting = hpConnectingBad; //default to bad connection
                if ( CheckConnectingPin( iHoverIndex , bHoverPin ) ) {
                    iTgtType = PinPositionFromObject( iHoverIndex , bHoverPin , &iXX , &iYY );
                    hpConnecting = hpConnectingGood; pbConnectingCursor = IDC_CONNECT;
                }
                if (iTgtType > iSrcType) { SWAP(iX,iXX) ; SWAP(iY,iYY) ; }

                _const hOldPen = SelectObject( hdc , hpConnecting );
                for (int iN=0; iN<3; iN++) {
                    const int iOX = (iN & 1), iOY = (iN/2);
                    const POINT atBezier[] = {
                        {iOX+iX         , iOY+iY} ,
                        {iOX+iX         , iOY+iYY} ,
                        {iOX+(iX+iXX)/2 , iOY+(iY+iYY)/2} ,
                        {iOX+iXX        , iOY+iYY}
                    };
                    PolyBezier( hdc , atBezier , 4 );
                }
                SelectObject( hdc , hOldPen );
            }
        }


        //puts("drawing end");
        //
        //

        bDrawn = 2;
        if (iViewY == iTempViewY) { bDrawn = 1; if (wParam) { KillTimer(hwnd,wParam); } }
        InvalidateRect( hwnd , NULL , true ); //UpdateWindow( hwnd );
        return;
    }; //void DrawWindow(void)

    int ObtainUniqueID( char* pbUsage) {
        static int iLastID = 1000;
        for (int i=0; i<16384; i++) {
            int iID = (iLastID + i) % 16384;
            //prevent iID of 0 (reserved) and check if it's already used
            if (iID && !(pbUsage[iID/8] & (1<<(iID%8)))) {
                printf("new ID: %i\n",iID);
                pbUsage[iID/8] |= (1<<(iID%8)); return iLastID=iID;
            }
        }
        return -1;
    }
    void ReleaseUniqueID( char* pbUsage , int iID) {
        printf("releasing ID: %i\n",iID);
        if (iID >= 0) pbUsage[iID/8] &= ~(1<<(iID%8));
    }

    int InsertObject( int iPosX , int iPosY , int iClassID ) {
        _const ptOrder = g_ptOrder;
        //increase storage if needed
        if (g_iObjCount >= g_iObjMaxCount) {
            g_iObjMaxCount += _MaxAllocationGap;
            g_ptOrder = realloc( g_ptOrder , g_iObjMaxCount*sizeof(*g_ptOrder) );
            printf("Reallocate (expand) to %i objects\n",g_iObjMaxCount);
            //todo: check for faillure
        }

        //put the index last in the list and then...
        //bubble it up till the right order (insertion sort)
        //since the list is sorted technically i could use binary search
        //to locate the position, but a "memmove" would still be required
        //to insert into the position, since this is not a linked list
        //and if this was a linked list then i could put the index into the
        //data itself, but it would need to be a double linked list. or a slow check
        int iNew;
        for (iNew = g_iObjCount ; iNew > 0 ; iNew--) {
            _with( aObject(iNew-1) ) {
              if ((_Grid2Pix(w->iY)) < iPosY) { break; }
              g_ptOrder[iNew] = g_ptOrder[iNew-1];
            } _endwith;
        }
        //initialize new slot
        _auto pClsInfo = &g_ClassInterface[iClassID];
        //printf("MinBytesConstructor: %i\n", pClsInfo->iMinBytesConstructor);
        g_ptOrder[iNew] = malloc(sizeof(**g_ptOrder)+pClsInfo->iMinBytesConstructor);
        _with( aObject(iNew) ) {
            w->iX = _Pix2Grid(iPosX); w->iW = _Pix2Grid(80);
            w->iY = _Pix2Grid(iPosY); w->iH = _Pix2Grid(36);
            w->uUniqueID = ObtainUniqueID(bUsedIDs);
            w->iClassID = iClassID;
            w->tConnector.uConnector = 0;
            sprintf(w->zName , pClsInfo->pzNameTemplate, iObjTotal+1 );
            _with( aObject_Content(iNew,ClsStringStruct) ) {
                w->iLength = 0; w->iBuffer = 1; w->zContent[0] = 0;
            } _endwith;
            if (_Grid2Pix(w->iX+w->iW) > iMaxX) { iMaxX = _Grid2Pix(w->iX+w->iW) ; iMaxXIdx = iNew ; ScrollUpdate( hwnd , -1 , - 1 ); }
            if (_Grid2Pix(w->iY+w->iH) > iMaxY) { iMaxY = _Grid2Pix(w->iY+w->iH) ; iMaxYIdx = iNew ; ScrollUpdate( hwnd , -1 , - 1 ); }
        } _endwith;
        g_iObjCount++; iObjTotal++;
        iSelectedIndex = iNew;
        SetUpdate();
        return iNew;
    }
    int RemoveObject( int iIndex ) {
        _const ptOrder = g_ptOrder;

        //don't remove if object is being dragged
        if ( iSelectedIndex == iIndex && bDragging ) {
            bDragging = 1 ; SendMessage( hwnd , WM_LBUTTONUP , 0 , 0 );
        }

        //grab end of X,Y to check if it was at limit of view area
        int iXX,iYY;
        _with( aObject(iIndex) ) {
            iXX = _Grid2Pix(w->iX + w->iW);
            iYY = _Grid2Pix(w->iY + w->iH);
            w->iW = 0;
            //free connectors
            if (w->tConnector.uConnector) {
                if (!w->tConnector.uOneConnector) {
                    free( g_pListConnectors[w->tConnector.uListID] );
                    g_pListConnectors[w->tConnector.uListID] = 0;
                    g_iListConnectorsCount--;
                }
            }
            w->tConnector.uConnector = 0;
            ReleaseUniqueID( bUsedIDs , w->uUniqueID );
        } _endwith;

        free( g_ptOrder[iIndex] ); g_ptOrder[iIndex] = 0;

        //removing last is simple but otherwise we need to close the gap
        g_iObjCount--;
        if (iIndex != g_iObjCount) {
            memmove( g_ptOrder+iIndex , g_ptOrder+iIndex+1 , sizeof(*g_ptOrder)*(g_iObjCount-iIndex) );
        }

        if (iIndex == iStartIdx) { iStartIdx++; }
        if (iIndex == iEndIdx) { iEndIdx--; }
        if (iStartIdx >= g_iObjCount) { iStartIdx = g_iObjCount-1; }
        if (iEndIdx >= g_iObjCount) { iEndIdx = g_iObjCount-1; }
        if (iStartIdx < 0) { iStartIdx = 0; }

        if ( iSelectedIndex == iIndex ) {
            if ( (iSelectedIndex > 0) ) { iSelectedIndex--; }
            if ( !g_iObjCount ) { iSelectedIndex = -1; }
            if (iSelectedIndex >= 0) {
                _with( aObject(iSelectedIndex) ) {
                    if ( (_Grid2Pix(w->iY)-iViewY) < 0 || (_Grid2Pix(w->iY+w->iH)-iViewY) > iBufHei ) {
                        //printf("New auto select: %i -> %i,%i\n" ,  iSelectedIndex , w->iX , w->iY);
                        SendMessage( hwnd , WM_VSCROLL , SB_THUMBTRACK , _Grid2Pix(w->iY)-(iBufHei/2) );
                    }
                    if ( (_Grid2Pix(w->iX)-iViewX) < 0 || (_Grid2Pix(w->iX+w->iW)-iViewX) > iBufWid ) {
                        SendMessage( hwnd , WM_HSCROLL , SB_THUMBTRACK , _Grid2Pix(w->iX)-(iBufWid/2) );
                    }
                } _endwith;
            } //endif
        } //endif

        //shrink if too much space left in order buffer...
        if ( g_iObjCount <= ((g_iObjMaxCount-_MaxAllocationGap)-((_MaxAllocationGap)/2)) ) {
            g_iObjMaxCount -= _MaxAllocationGap;
            g_ptOrder = realloc( g_ptOrder , g_iObjMaxCount*sizeof(*g_ptOrder) );
            printf("Reallocate (srhink) to %i objects\n",g_iObjMaxCount);
            //todo: check for faillure
        }

        //if deleted piece was on the workarea limit, recalculate the limit.
        if ( (iXX == iMaxX) || (iYY == iMaxY) ) { iMaxXIdx = -1 ; ScrollUpdate( hwnd , -1 , - 1 ); }

        SetUpdate();
        return 1;
    }
    int ObjectFromPoint( POINT pt , /*OUT*/ RECT* pRect ) {
        _const ptOrder = g_ptOrder;
        static int iCachedIndex = -1;
        //if no objects, clear cache and return
        if ((!g_ptOrder) || ( !g_iObjCount )) { return iCachedIndex = -1; }
        //if index is cached and valid, use it first
        if ( (iCachedIndex != -1) && (iCachedIndex < g_iObjCount) ) {
            _with( aObject(iCachedIndex) ) {
                const RECT tRc = {
                    .left  = (_Grid2Pix(w->iX)-iViewX)-cBlkBrd*2       , .top    = (_Grid2Pix(w->iY)-iViewY)-cBlkBrd*2 ,
                    .right = (_Grid2Pix(w->iX+w->iW)-iViewX)+cBlkBrd*2 , .bottom = (_Grid2Pix(w->iY+w->iH)-iViewY)+cBlkBrd*2 };
                if (PtInRect( &tRc , pt )) {
                    if (pRect) *pRect = tRc;
                    return iCachedIndex;
                }
            } _endwith;
        }
        //otherwise, scan all objects
        for ( int iIndex = iEndIdx ; iIndex>=iStartIdx ; iIndex-- ) {
            _with( aObject(iIndex) ) {
                const RECT tRc = {
                    .left   = (_Grid2Pix(w->iX)-iViewX)-cBlkBrd*2 ,
                    .top    = (_Grid2Pix(w->iY)-iViewY)-cBlkBrd*2 ,
                    .right  = (_Grid2Pix(w->iX+w->iW)-iViewX)+cBlkBrd*2 ,
                    .bottom = (_Grid2Pix(w->iY+w->iH)-iViewY)+cBlkBrd*2
                };
                if (PtInRect( &tRc , pt )) {
                    //printf("found %d\n", iIndex);
                    if (pRect) *pRect = tRc;
                    return iCachedIndex = iIndex;
                }
            } _endwith;
        }
        return iCachedIndex = -1;
    }

    LRESULT CALLBACK CtlEditProc( HWND hwnd , UINT uMsg , WPARAM wParam , LPARAM lParam ) {
        if ((uMsg == WM_KEYDOWN)) {
            int iID=0;
            if (wParam == VK_ESCAPE) { iID=1 ; wParam=VK_RETURN; }
            if (wParam == VK_RETURN) {
                if (GetKeyState( VK_CONTROL ) >= 0) {
                return SendMessage( GetParent( hwnd ) , WM_COMMAND , MAKEWPARAM(iID,EN_KILLFOCUS) , (LPARAM)hwnd );
                }
            }
        }
        return CallWindowProc( hwndOrgProc , hwnd , uMsg , wParam , lParam );
    }
    void BeginEndEdit( int iIndex /* =0 */ , const RECT* pRC /* = NULL */ ) {
        if (pRC && !hwndEdit) {
            const int iWid=pRC->right-pRC->left, iHei=pRC->bottom-pRC->top, iBorderUD = (iHei)/4;
            int cStyle = WS_CHILD | WS_BORDER | ES_AUTOHSCROLL | ES_WANTRETURN;
            int iTop, iBottom;
            if ((iHei+(iBorderUD/4)) > iFontSize*5) {
                iTop = (pRC->top+2)+iBorderUD; iBottom = pRC->bottom-(iBorderUD+2);
                cStyle |= ES_MULTILINE | ES_AUTOVSCROLL;
            } else {
                iTop = pRC->top+((iHei-iFontSize)/2); iBottom = iTop + iFontSize+2;
            }
            _with( *pRC ) {
                hwndEdit = CreateWindowEx( 0 , "EDIT" , NULL , cStyle , pRC->left+4, iTop, iWid-8, iBottom-iTop , hwnd , NULL , NULL , NULL );
            } _endwith;
            //CM_BeginEdit: { //wParam = hCtlEdit // lParam = (POINTS)tClick
            LPARAM const tClick = GetMessagePos(); ppEditObj = g_ptOrder+iIndex;
            //DiagramObjectStruct* pTemp = *ppEditObj; printf("%p = %p\n",pTemp->Content, w->Content);
            const _auto bProceed = g_ClassInterface[(*ppEditObj)->iClassID].pfHandlerProc( (*ppEditObj)->Content , NULL , CM_BeginEdit , (WPARAM)hwndEdit , (LPARAM)tClick );
            if (!bProceed) { DestroyWindow( hwndEdit ); hwndEdit = NULL ; ppEditObj = NULL; return; }
            hwndOrgProc = (void*)SetWindowLongPtr( hwndEdit , GWLP_WNDPROC , (LONG_PTR)CtlEditProc );
            SendMessage( hwndEdit , WM_SETFONT , (WPARAM)hCtlFont , 0);
            ShowWindow( hwndEdit , SW_SHOW ); SetFocus( hwndEdit );
        } else {
            if (hwndEdit) {
                int iEndOrCancel = iIndex ? CM_CancelEdit : CM_EndEdit;
                g_ClassInterface[(*ppEditObj)->iClassID].pfHandlerProc( (*ppEditObj)->Content , NULL ,  iEndOrCancel , (WPARAM)hwndEdit , (LPARAM)ppEditObj );
                HWND hwndTemp = hwndEdit; hwndEdit = NULL ; ppEditObj = NULL;
                DestroyWindow( hwndTemp ); SetUpdate();
            }
        }
    }

    void RetrieveDiagram( DiagramFileStruct* pNewDiagram ) { //update current diagram state from the new file structure
        if (pNewDiagram) {
            SCROLLINFO tInfo = { .cbSize = sizeof(tInfo) , .fMask = SIF_POS };
            pDiagram = pNewDiagram;
            _with( *pDiagram ) {
                g_ptOrder = w->pObjects;
                g_iObjCount = w->iObjectCount; g_iObjMaxCount = w->iObjectMaxCount;
                //restore from cached (when it makes sense)
                iSelectedIndex = w->iSelectedIdx;
                iMaxXIdx = iMaxYIdx = -1;
                iStartIdx = 0 ; iEndIdx = -1;
                iViewX = w->iViewX; iViewY = w->iViewY;
                ScrollUpdate( hwnd , -1 , -1 );
                tInfo.nPos = iViewX ; SetScrollInfo( hwnd , SB_HORZ , &tInfo , TRUE );
                tInfo.nPos = iViewY ; SetScrollInfo( hwnd , SB_VERT , &tInfo , TRUE );
                memcpy( bUsedIDs , w->bUsedIDs , sizeof(bUsedIDs) );
                SetUpdate();
            } _endwith
        }
    }
    void WriteBackDiagram() { //save current diagram state back into the file structure
        if (pDiagram) {
            _with( *pDiagram ) {
                w->pObjects = g_ptOrder;
                w->iObjectCount = g_iObjCount;
                w->iObjectMaxCount = g_iObjMaxCount;
                ///update view cache
                w->iViewX = iViewX;
                w->iViewY = iViewY;
                w->iSelectedIdx = iSelectedIndex;
                memcpy( w->bUsedIDs , bUsedIDs , sizeof(bUsedIDs) );
                //GetScrollInfo( hwnd , SB_HORZ , &tInfo); w->iPosH = tInfo.nPos;
                //GetScrollInfo( hwnd , SB_VERT , &tInfo); w->iPosV = tInfo.nPos;
            } _endwith
        }
    }

    DiagramFileStruct* CreateTestDiagram( char* pzName ) {

        //#define aObject_Content(_I,_T) (*((_T*)(pObjects[_I]->Content)))

        DiagramFileStruct* pFile = calloc( 1 , sizeof( DiagramFileStruct ) );
        if (!pFile) return NULL;

        pFile->iObjectMaxCount = _MaxAllocationGap;
        DiagramObjectStruct** ptOrder = pFile->pObjects = malloc(pFile->iObjectMaxCount*sizeof(*g_ptOrder));
        if (!ptOrder) { free(pFile); return NULL; }

        //initialize file name
        strncpy( pFile->zName , pzName , _countof(pFile->zName) );

        int iPosY=8/_GridSize, iPosX=0 , iObjCount = 0;
        { //sample string
            ptOrder[iObjCount] = malloc(sizeof(**ptOrder)+sizeof(ClsStringStruct)+12);
            _with( aObject(iObjCount) ) {
                w->iX = 8/_GridSize ; w->iW = 128/_GridSize;
                w->iY = iPosY       ; w->iH = 48/_GridSize;
                w->iClassID = idClsString;
                w->uUniqueID = ObtainUniqueID( pFile->bUsedIDs );
                w->tConnector.uConnector = 0;
                //w->tConnector.uIndexHint = 1; w->tConnector.uUniqueID = 2; w->tConnector.uTargetPin = 1;
                strncpy( w->zName , "MyString" , _countof(w->zName) );
                iPosY += w->iH+24/_GridSize;
            } _endwith;
            _with( aObject_Content(iObjCount,ClsStringStruct) ) {
                w->iLength = 11; w->iBuffer = 12;
                strcpy( w->zContent , "Hello World" );
            } _endwith;
            iObjCount++;
        }
        { //sample device
            ptOrder[iObjCount] = malloc(sizeof(**ptOrder)+sizeof(ClsStdOutStruct));
            _with( aObject(iObjCount) ) {
                w->iX = 8/_GridSize ; w->iW = 128/_GridSize;
                w->iY = iPosY       ; w->iH = 48/_GridSize;
                w->uUniqueID = ObtainUniqueID( pFile->bUsedIDs );
                w->tConnector.uConnector = 0;
                w->iClassID = idClsStdOut;
                strncpy( w->zName , "STDOUT" , _countof(w->zName) );
                iPosY += w->iH+8/_GridSize;
            } _endwith;
            _with( aObject_Content(iObjCount,ClsStdOutStruct) ) {
            } _endwith;

            iObjCount++;
        }

        InsertConnectionPin( iObjCount-2 , 1 , iObjCount-1 , 1 );
        InsertConnectionPin( iObjCount-1 , 1 , iObjCount-2 , 1 );

        pFile->iObjectCount = iObjCount;
        return pFile;

        //#undef aObject_Content
    }
    DiagramFileStruct* CreateRandomTestDiagram( char* pzName ) {

        //#define aObject_Content(_I,_T) (*((_T*)(pObjects[_I]->Content)))

        DiagramFileStruct* pFile = calloc( 1 , sizeof( DiagramFileStruct ) );
        if (!pFile) return NULL;

        pFile->iObjectMaxCount = _MaxAllocationGap;
        DiagramObjectStruct** ptOrder = pFile->pObjects = malloc(pFile->iObjectMaxCount*sizeof(*g_ptOrder));
        if (!ptOrder) { free(pFile); return NULL; }

        strncpy( pFile->zName , pzName , _countof(pFile->zName) );

        int iPosY=8/_GridSize, iObjCount = 0;
        for (int iN = 0 ; iN < _rnd(_MaxAllocationGap) ; iN++) {
            int iPosX=_rnd(16);
            if (rand()&1) { //sample string
                int iLen = 1+_rnd(16), iWid = 48/_GridSize+iLen;
                if (iWid > 255) iWid = 255;
                ptOrder[iObjCount] = malloc(sizeof(**ptOrder)+sizeof(ClsStringStruct)+iLen+1);
                _with( aObject(iObjCount) ) {
                    w->iX = iPosX ; w->iW = iWid;
                    w->iY = iPosY ; w->iH = 48/_GridSize+_rnd(4);
                    w->tConnector.uConnector = 0;
                    w->iClassID = idClsString;
                    sprintf( w->zName , "Str%02d:%02d" , iLen , iObjCount );
                    iPosY += w->iH+(1+_rnd(4));
                } _endwith
                _with( aObject_Content(iObjCount,ClsStringStruct) ) {
                    w->iLength = iLen; w->iBuffer = iLen+1;
                    for (int i = 0 ; i < iLen ; i++) {
                        w->zContent[i] = (rand()&1) ? 'a' + _rnd(26) : '0' + _rnd(10);
                    }
                    w->zContent[iLen] = 0;
                } _endwith
            } else { //sample device
                ptOrder[iObjCount] = malloc(sizeof(**ptOrder)+sizeof(ClsStdOutStruct));
                _with( aObject(iObjCount) ) {
                    w->iX = iPosX       ; w->iW = 128/_GridSize;
                    w->iY = iPosY       ; w->iH = 48/_GridSize+_rnd(4);
                    w->iClassID = idClsStdOut;
                    sprintf( w->zName , "STDOUT%02d" , iObjCount );
                    iPosY += w->iH+(1+_rnd(4));
                } _endwith
                _with( aObject_Content(iObjCount,ClsStdOutStruct) ) {
                    //
                } _endwith
            }
            iObjCount++;
        }

        pFile->iObjectCount = iObjCount;
        return pFile;

        //#undef aObject_Content
    }

    // ------------- Message dispatch --------------
    _const ptOrder = g_ptOrder;
    switch (message) {
        case WM_ERASEBKGND: { return 1; }
        case WM_SETCURSOR: {
            //keep resizing cursor active while resizing
            if (bResizing) { SetCursor( LoadCursor( NULL , pbCurCursor ) ); return 0; }
            pbCurCursor = 0; bLastSizeSide = rsNone; bHoverPin = 0; //reset cursor
            //if dragging show moving cursor
            if (bDragging>1) { SetCursor( LoadCursor( NULL , pbCurCursor=IDC_SIZEALL ) ); return 0; }
            //if not check if hovering over a visible object
            const POINT pt = { iMouseX , iMouseY }; RECT tRc;
            int iIndex = iHoverIndex = ObjectFromPoint( pt , &tRc );
            if (iIndex < 0 && bConnectingPin) { SetCursor( LoadCursor( NULL , IDC_CONNECT ) ); return 0; }
            if (iIndex >= 0) {
                _with( aObject(iIndex) ) {
                    const RECT tRc = {
                        .left   = _Grid2Pix(w->iX)-iViewX ,
                        .top    = _Grid2Pix(w->iY)-iViewY ,
                        .right  = _Grid2Pix(w->iX+w->iW)-iViewX ,
                        .bottom = _Grid2Pix(w->iY+w->iH)-iViewY
                    };
                    if (1) /*(PtInRect( &tRc , pt ))*/ {
                        char bSides = 0;
                        pbCurCursor = IDC_HAND; //default cursor
                        //if mouse is near an edge to set resize cursor
                        if (1) { //} iSelectedIndex == iIndex) {
                            if (pt.x < tRc.left+6)   { bLastSizeSide |= rsLeft; }
                            if (pt.x > tRc.right-6)  { bLastSizeSide |= rsRight; }
                            if (pt.y < tRc.top+6)    { bLastSizeSide |= rsTop; }
                            if (pt.y > tRc.bottom-6) { bLastSizeSide |= rsBottom; }
                            if (bLastSizeSide) {
                                int iWid = tRc.right-tRc.left, iHei = tRc.bottom-tRc.top;
                                int iPinCnt=g_ClassInterface[w->iClassID].bInPins, iCurPin=1;
                                //if the edge contains a pin set the cursor to "connect"
                                if ( ( bLastSizeSide & rsTop ) && (pt.y <= tRc.top) ) { //check for input pins
                                    int iPinSpace = (iWid)/(iPinCnt+1);
                                    for (int i=tRc.left+iPinSpace+(iFontSize/4),n=0 ; iPinCnt-- ; i += iPinSpace, n++) {
                                        if (abs(pt.x-i) < (iFontSize/2)) { pbCurCursor = IDC_CONNECT; bLastSizeSide = 0; bHoverPin=iCurPin+n; break; }
                                    }
                                }
                                iCurPin += iPinCnt; iPinCnt=g_ClassInterface[w->iClassID].bOutPins;
                                if ( ( bLastSizeSide & rsBottom ) && (pt.y >= tRc.bottom) ) { //check for output pins
                                    int iPinSpace = (iWid)/(iPinCnt+1);
                                    for (int i=tRc.left+iPinSpace+(iFontSize/4),n=0 ; iPinCnt-- ; i += iPinSpace, n++) {
                                        if (abs(pt.x-i) < (iFontSize/2)) { pbCurCursor = IDC_CONNECT; bLastSizeSide = 0; bHoverPin=iCurPin+n; break; }
                                    }
                                }
                                iCurPin += iPinCnt; iPinCnt=g_ClassInterface[w->iClassID].bExecPins;
                                if ( ( bLastSizeSide & rsRight ) && (pt.x >= tRc.right) ) { //check for exec pins
                                    int iPinSpace = (iHei)/(iPinCnt+1);
                                    for (int i=tRc.top+iPinSpace+(iFontSize/4),n=0 ; iPinCnt-- ; i += iPinSpace, n++) {
                                        if (abs(pt.y-i) < (iFontSize/2)) { pbCurCursor = IDC_CONNECT; bLastSizeSide = 0; bHoverPin=iCurPin+n; break; }
                                    }
                                }
                                //if connecting a pin can't have sizing cursors
                                //but the cursor for that can vary if the pin connected or not;
                                if (bLastSizeSide ) {
                                    //otherwise set the cursor to "resize"
                                    static char* const pbSideToCursor[] = {
                                        [rsLeft] = IDC_SIZEWE, [rsRight]  = IDC_SIZEWE,
                                        [rsTop]  = IDC_SIZENS, [rsBottom] = IDC_SIZENS,
                                        [rsTop|rsLeft]    = IDC_SIZENWSE, [rsTop|rsRight]    = IDC_SIZENESW,
                                        [rsBottom|rsLeft] = IDC_SIZENESW, [rsBottom|rsRight] = IDC_SIZENWSE
                                    };
                                    pbCurCursor = pbSideToCursor[bLastSizeSide];
                                }
                            }
                        }
                        //if connecting then the cursor is determined by a delayed check
                        //but if the hovered object has only one pin then that pin is selected
                        //TODO: improve this so that if there's more pins then set the first pin based on the side
                        if (bConnectingPin) {
                            if ( !bHoverPin ) {
                                _const iPins = g_ClassInterface[w->iClassID].bInPins +
                                    g_ClassInterface[w->iClassID].bOutPins + g_ClassInterface[w->iClassID].bExecPins;
                                if (iPins==1) { bHoverPin = 1; }
                            }
                            pbCurCursor = pbConnectingCursor;
                        }

                        if (pbCurCursor) { SetCursor( LoadCursor( NULL , pbCurCursor ) ); return 0; }
                    }
                } _endwith;
            }
            //otherwise DefWindowProc will set default cursor
            break;
        }
        case WM_MOUSEMOVE: {       //Mouse moved in the control
            iMouseX = (short)LOWORD(lParam);  // horizontal position of cursor
            iMouseY = (short)HIWORD(lParam);  // vertical position of cursor
            char bMoved = 0;
            int iNewX,iNewY;

            if (bConnectingPin) {
                Diagram_WndProc( hwnd , WM_SETCURSOR , HTCLIENT, lParam );
                SetUpdate();
            }
            if (bHoverPin != bPrevPin) {
                //printf("idx=%i pin=%i\n",iHoverIndex,bHoverPin);
                bPrevPin = bHoverPin; SetUpdate();
            }

            //if resizing, check sides and adjust new size
            if (bResizing) {
                _with( aObject(iSelectedIndex) ) {
                    //calculate new position/size (adjusted to grid, for bResizing sides)
                    //if left or top, adjust new position, if right or bottom, adjust new size
                    int iL=(w->iX)  , iT=(w->iY), iW = (w->iW), iH = (w->iH);
                    if (bResizing & rsLeft)   { iL = _Pix2Grid((iMouseX+iViewX)); iW += (w->iX-iL); }
                    if (bResizing & rsTop)    { iT = _Pix2Grid((iMouseY+iViewY)); iH += (w->iY-iT); }
                    if (bResizing & rsRight)  { iW = _Pix2Grid((iMouseX+iViewX+(_GridSize/2)))-iL; } //4 = Half GridSize
                    if (bResizing & rsBottom) { iH = _Pix2Grid((iMouseY+iViewY+(_GridSize/2)))-iT; }
                    if (iL<0) { iL = 0; }; if (iT<0) { iT = 0; }
                    if (iW < _BlkMinWid) { if (bResizing & rsLeft) { iL = w->iX+w->iW-_BlkMinWid; } ; iW = _BlkMinWid; }
                    else if (iW > _BlkMaxWid) { if (bResizing & rsLeft) { iL = w->iX+w->iW-_BlkMaxWid; } ; iW = _BlkMaxWid; }
                    if (iH < _BlkMinHei) { if (bResizing & rsTop)  { iT = w->iY+w->iH-_BlkMinHei; } ; iH = _BlkMinHei; }
                    else if (iH > _BlkMaxHei) { if (bResizing & rsTop)  { iT = w->iY+w->iH-_BlkMaxHei; } ; iH = _BlkMaxHei; }
                    if ( (iL != w->iX) || (iT != w->iY) || (iW != w->iW) || (iH != w->iH) ) { //update position/size
                        if ( (iL != w->iX) || (iT != w->iY) ) { bMoved = 1; iNewX = iL; iNewY = iT; }
                        w->iX = iL; w->iY = iT; w->iW = iW; w->iH = iH;
                        SetUpdate();
                    }
                } _endwith;
            }

            //check if moved enough to start a drag, to active it and backup initial position
            if ((bDragging==1) && ((abs(iMouseX-iDragStartX)>3) || (abs(iMouseY-iDragStartY)>3))) {
                iDragCancelX = _Grid2Pix(aObject(iSelectedIndex).iX);
                iDragCancelY = _Grid2Pix(aObject(iSelectedIndex).iY);
                bDragging = 2; SetCursor( LoadCursor( NULL , pbCurCursor=IDC_SIZEALL ) );
            }
            //if dragging move the block aligned to the grid;
            if (bDragging==2) {
                _with( aObject(iSelectedIndex) ) {
                    iNewX = _Pix2Grid((iDragCancelX+(iMouseX-iDragStartX))+3);
                    iNewY = _Pix2Grid((iDragCancelY+(iMouseY-iDragStartY))+3);
                    if (iNewX<0) { iNewX = 0; }; if (iNewY<0) { iNewY = 0; }
                    if ((iNewX != w->iX) || (iNewY != w->iY)) {
                        w->iX = iNewX; w->iY= iNewY; bMoved = 1;
                        SetUpdate();
                    } //endif
                } _endwith;
            } //endif (bDragging==2)

            if (bMoved) { //if moved then may reorder,scroll
                while (1) { //reorder the dragging object
                    if (iSelectedIndex < (g_iObjCount-1)) {
                        const int iNextY = aObject(iSelectedIndex+1).iY;
                        if ( (iNewY > iNextY) || ((iNewY==iNextY) && (iNewX > aObject(iSelectedIndex+1).iX)) ) {
                            SWAP( g_ptOrder[iSelectedIndex] , g_ptOrder[iSelectedIndex+1] ); iSelectedIndex++; continue;
                        } //endif
                    } //endif
                    if (iSelectedIndex > 0) {
                        const int iPrevY = aObject(iSelectedIndex-1).iY;
                        if ( (iNewY < iPrevY) || ((iNewY==iPrevY) && (iNewX < aObject(iSelectedIndex-1).iX)) ) {
                            SWAP( g_ptOrder[iSelectedIndex] , g_ptOrder[iSelectedIndex-1] ); iSelectedIndex--; continue;
                        } //endif
                    } //endif (iSelectedIndex > 0) {
                    break;
                } //wend
                _with( aObject(iSelectedIndex) ) {
                    bool bUpdate=0;
                    if (_Grid2Pix(w->iX+w->iW) > iMaxX) { iMaxX = _Grid2Pix(w->iX+w->iW); iMaxXIdx=iSelectedIndex; bUpdate=true; }
                    if (_Grid2Pix(w->iY+w->iH) > iMaxY) { iMaxY = _Grid2Pix(w->iY+w->iH); iMaxYIdx=iSelectedIndex; bUpdate=true; }
                    if (bUpdate) { ScrollUpdate(hwnd,-1,-1); }
                } _endwith;
                // if mouse is outside of visible area then scroll it
                static char bSwap ; bSwap = (bSwap+1) & 3;
                if (!bSwap) {
                    RECT tRc; GetClientRect( hwnd , &tRc );
                    if (iMouseY < 0)           { PostMessage( hwnd , WM_VSCROLL , SB_LINEUP   , 0 ); }
                    if (iMouseY >= tRc.bottom) { PostMessage( hwnd , WM_VSCROLL , SB_LINEDOWN , 0 ); }
                    if (iMouseX < 0)           { PostMessage( hwnd , WM_HSCROLL , SB_LINEUP   , 0 ); }
                    if (iMouseX >= tRc.right)  { PostMessage( hwnd , WM_HSCROLL , SB_LINEDOWN , 0 ); }
                } //endif (bSwap)
            } //endif (bMoved)

            return 0;
        }
        case WM_PAINT: {           //Update window from bitmap if ready
            if (bDrawn <= 0) { ValidateRect( hwnd , NULL ); return 0; }
            HDC hdc = (HDC)wParam; // the device context to draw in

            PAINTSTRUCT tPaint;
            if (!wParam) {
                BeginPaint( hwnd , &tPaint ); hdc = tPaint.hdc;
            } else {
                GetClientRect( hwnd , &tPaint.rcPaint );
            }

            _with(tPaint.rcPaint) {
                BitBlt( hdc , w->left , w->top , w->right-w->left , w->bottom-w->top , hDcBuffer , w->left , w->top , SRCCOPY );
            } _endwith

            if (!wParam) { EndPaint( hwnd , &tPaint ); }
            return 0;
        }
        case WM_SIZE: {            //Window Size changed discard bitmap
            if (wParam == SIZE_MINIMIZED) { break; }  // resizing flag
            int nWid = LOWORD(lParam);  // width of client area
            int nHei = HIWORD(lParam); // height of client area
            if ( (nWid > iBufWid) || (nWid <= (iBufWid-64)) || (nHei > iBufHei) || (nHei <= (iBufHei-64)) ) {
                SendMessage( hwnd , DIM_CREATE_BUFFER , 0 , lParam );
                SetUpdate();
            }
            return 0;
        }
        case WM_HSCROLL:
        case WM_VSCROLL: {
            _const SB_ = (message==WM_VSCROLL ? SB_VERT : SB_HORZ);
            int nScrollCode = (int)LOWORD(wParam); // scroll bar value
            SCROLLINFO tInfo = { .cbSize = sizeof(SCROLLINFO) , .fMask = SIF_ALL };
            if (!GetScrollInfo( hwnd , SB_ , &tInfo )) {
                puts("Failed to get diagram scroll info");
            }
            if (lParam) {
                if (lParam < tInfo.nMin) { lParam = tInfo.nMin; }
                if (lParam > tInfo.nMax) { lParam = tInfo.nMax; }
                tInfo.nTrackPos = lParam;
            }

            int bFast=1, nPos = tInfo.nPos;
            switch (nScrollCode) {
                case SB_TOP:           { tInfo.nPos = tInfo.nMin; break; }
                case SB_BOTTOM:        { tInfo.nPos = tInfo.nMax; break; }
                case SB_ENDSCROLL:     { break; }
                case SB_LINEDOWN:      { tInfo.nPos = min( nPos+32 , tInfo.nMax ); break; }
                case SB_LINEUP:        { tInfo.nPos = max( nPos-32 , tInfo.nMin ); break; }
                case SB_PAGEDOWN:      { tInfo.nPos = min( nPos+tInfo.nPage , tInfo.nMax ); break; }
                case SB_PAGEUP:        { tInfo.nPos = max( nPos-tInfo.nPage , tInfo.nMin ); break; }
                case SB_THUMBPOSITION: { break; }
                case SB_THUMBTRACK:    { bFast=0; tInfo.nPos = tInfo.nTrackPos; break; }
            }

            //printf("{%i}\n",tInfo.nPos);
            tInfo.fMask = SIF_POS;
            if (!SetScrollInfo( hwnd , SB_ , &tInfo , true ) && tInfo.nPos) {
                printf("Failed to set diagram scroll info: %i->%i\n",nPos,tInfo.nPos);
            }
            //if (tInfo.nPos==tInfo.nMin)             { EnableScrollBar( hwnd , SB_ , ESB_DISABLE_LTUP ); }
            //if (tInfo.nPos>=tInfo.nMax-tInfo.nPage) { EnableScrollBar( hwnd , SB_ , ESB_DISABLE_RTDN ); }
            if (nPos != tInfo.nPos) {
                if (bDragging) {
                    GetScrollInfo( hwnd , SB_ , &tInfo );
                    if (message==WM_VSCROLL) { iDragStartY += (nPos-tInfo.nPos); } else { iDragStartX += (nPos-tInfo.nPos); }
                }
                if (nPos == tInfo.nMin)          { EnableScrollBar( hwnd , SB_ , ESB_ENABLE_BOTH ); }
                if (nPos>=tInfo.nMax-tInfo.nPage){ EnableScrollBar( hwnd , SB_ , ESB_ENABLE_BOTH ); }
                if (bFast) { SetUpdate(); } else { SetUpdateAsync(); }
            }
            return 0;
        }
        case WM_TIMER: {           //TIMER events (REDRAW!)
            WindowDraw();
            return 0;
        }
        case DIM_CREATE_BUFFER: {  //INTERNAL: recreate bitmap buffer
            int nWid = LOWORD(lParam);  // width of client area
            int nHei = HIWORD(lParam); // height of client area
            iBufWid = 64+((nWid | 63) & (~63)); //gives a room of -63 to 63 extra pixels
            iBufHei = 64+((nHei | 63) & (~63));
            HDC hdc = GetDC( hwnd );
            if (!hDcBuffer) {
                hDcBuffer = CreateCompatibleDC( hdc );
                SelectObject( hdc , hCtlFont );
            }
            hBmBuffer = CreateCompatibleBitmap( hdc , iBufWid , iBufHei );
            DeleteObject( SelectObject( hDcBuffer , hBmBuffer ) );
            ScrollUpdate( hwnd , nWid , nHei );
            SetUpdate();
            return 1;
        }
        case DIM_SELECT: {         //lParam = DiagramFileStruct*
            LRESULT lRes = (LRESULT)pDiagram;
            WriteBackDiagram();
            RetrieveDiagram( (DiagramFileStruct*)lParam );
            return lRes;
            break;
        }
        case DIM_SETCLASS: {       //wParam = ClassId (sets class to be added when inserting a new object)
            iClassToAdd = (int)wParam;
            break;
        }
        case DIM_GENERATE: {       //wParam = FileCount // lParam = DiagramFileStruct**
            WriteBackDiagram();
            GenerateFullCode( (int)wParam , (DiagramFileStruct**)lParam );
            break;
        }
        case DIM_TESTFILE: {       //wParam = IsRandom // lParam = Name
            return (LRESULT) ((wParam) ? CreateRandomTestDiagram( (char*)lParam ) : CreateTestDiagram( (char*)lParam ));
            break;
        }
        case WM_MOUSEWHEEL: {      //Mouse wheel event
            int zDelta = (short) HIWORD(wParam);    // wheel rotation
            //fwKeys = LOWORD(wParam);    // key flags
            //xPos = (short) LOWORD(lParam);    // horizontal position of pointer
            //yPos = (short) HIWORD(lParam);    // vertical position of pointer
            SCROLLINFO tInfo = { .cbSize = sizeof(SCROLLINFO) , .fMask = SIF_ALL };
            GetScrollInfo( hwnd , SB_VERT , &tInfo );
            const int nPos = tInfo.nPos;
            tInfo.nPos = min( tInfo.nPos+zDelta/-2 , tInfo.nMax );
            tInfo.fMask = SIF_POS;
            SetScrollInfo( hwnd , SB_VERT , &tInfo , true );
            if (bDragging) {
                GetScrollInfo( hwnd , SB_VERT , &tInfo );
                iDragStartY += (nPos-tInfo.nPos);
                SendMessage( hwnd , WM_MOUSEMOVE , 0 , MAKELPARAM( iMouseX , iMouseY ));
            }
            SetUpdate();
            return 0;
        }
        case WM_CREATE: {          //Initialize control

            initComponents(); //basedecl.h

            hSmallFont = GetStockObject( SYSTEM_FONT );
            hbBack = CreateSolidBrush( cBack );
            hbBackGrid = CreateHatchBrush( HS_CROSS , cGrid );
            hpSelected = CreatePen( PS_SOLID , 3 , cSelected );
            hpConnectingBad  = CreatePen( PS_DOT , 0 , cConnectingBad );
            hpConnectingGood = CreatePen( PS_DOT , 0 , cConnectingGood );
            PostMessage( hwnd , WM_HSCROLL , 0,0 );
            PostMessage( hwnd , WM_VSCROLL , 0,0 );
            return 1;
        }
        case WM_SETFONT: {         //Set New Font
            if (hSmallFont) { DeleteObject( hSmallFont ); DeleteObject( hSmallFontB ); hSmallFont = hSmallFontB = NULL; }
            hCtlFont = (HFONT)wParam; SetUpdate();
            //SelectObject( hDcBuffer , hCtlFont );
            LOGFONT tFont; GetObject( hCtlFont , sizeof(tFont) , &tFont );
            int iFontHeight = tFont.lfHeight, iFontWidth = tFont.lfWidth;
            tFont.lfHeight = (tFont.lfHeight*2)/3;
            tFont.lfWidth  = (tFont.lfWidth *2)/3;
            hSmallFont = CreateFontIndirect( &tFont );
            tFont.lfWeight = FW_BOLD;
            hSmallFontB = CreateFontIndirect( &tFont );
            tFont.lfHeight = iFontHeight; tFont.lfWidth  = iFontWidth;
            tFont.lfWeight = FW_NORMAL;
            strcpy(tFont.lfFaceName,"Wingdings 3");
            hSymFont = CreateFontIndirect( &tFont );
            SetUpdate();
            return 0;
        }
        case WM_GETFONT: {         //Retrieve Current Font
            return (LRESULT)hCtlFont;
        }
        case WM_KEYDOWN: {         //Key pressed
            //printf("Keydown: %i\n",wParam);
            switch (wParam) {
                case VK_UP    : { return SendMessage( hwnd , WM_VSCROLL , SB_LINEUP   , 0); }
                case VK_DOWN  : { return SendMessage( hwnd , WM_VSCROLL , SB_LINEDOWN , 0); }
                case VK_PRIOR : { return SendMessage( hwnd , WM_VSCROLL , SB_PAGEUP   , 0); }
                case VK_NEXT  : { return SendMessage( hwnd , WM_VSCROLL , SB_PAGEDOWN , 0); }
                case VK_HOME  : { return (GetKeyState(VK_CONTROL) << 1) ? SendMessage( hwnd , WM_VSCROLL , SB_TOP    , 0) : 0; }
                case VK_END   : { return (GetKeyState(VK_CONTROL) << 1) ? SendMessage( hwnd , WM_VSCROLL , SB_BOTTOM , 0) : 0; }
                case VK_LEFT  : { return SendMessage( hwnd , WM_HSCROLL , SB_LINEUP   , 0); }
                case VK_RIGHT : { return SendMessage( hwnd , WM_HSCROLL , SB_LINEDOWN , 0); }
                case VK_INSERT: {
                    POINT pt = {.x = iMouseX, .y = iMouseY};
                    int idx = ObjectFromPoint(pt,NULL);
                    if (idx != -1) { iClassToAdd = aObject(idx).iClassID;
                        int iID = GetWindowLong( hwnd , GWL_ID );
                        SendMessage( GetParent(hwnd) , WM_COMMAND , MAKEWPARAM(iID,0) , iClassToAdd );
                        return 0;
                    }
                    if (iClassToAdd < 1) {
                        MessageBox( hwnd , "No class selected" , "Error" , MB_ICONINFORMATION );
                        return 0;
                    }
                    InsertObject( iViewX+iMouseX , iViewY+iMouseY , iClassToAdd );
                    break;
                }
                case VK_DELETE: {
                    if (iSelectedIndex != -1) { RemoveObject( iSelectedIndex ); }
                    break;
                }
            } //switch
            return 0;
        }
        case WM_LBUTTONDOWN: {     //Button pressed
            SetFocus(hwnd);
            int iOldSel = iSelectedIndex ; iSelectedIndex = -1; bConnectingPin = 0;
            static uint32_t iPrevTime=0;
            uint32_t iElapsed = GetMessageTime(); //printf("%i\n",iElapsed);
            iElapsed -= iPrevTime; iPrevTime = GetMessageTime();
            //printf("%i to %i\n",iStartIdx,iEndIdx);
            const POINT pt = { (short)LOWORD(lParam) , (short)HIWORD(lParam) }; RECT tRc;
            if ( (iSelectedIndex = ObjectFromPoint( pt , &tRc )) != -1 ) {
                if (bLastSizeSide) {
                    bResizing = bLastSizeSide; SetCapture(hwnd);
                    SetUpdate(); return 0;
                }
                _with( aObject(iSelectedIndex) ) {
                    if (iOldSel != iSelectedIndex) {
                        printf("Selected %i at %i,%i\n",iSelectedIndex,w->iX,w->iY);
                    } else {
                        if (iElapsed<300) {
                            iPrevTime -= 300;
                            printf("Editing %i at %i,%i\n",iSelectedIndex,w->iX,w->iY);
                            BeginEndEdit( iSelectedIndex , &tRc );
                            }
                        //return 0;
                    }
                } _endwith;
            }
            if (iOldSel != iSelectedIndex) { SetUpdate(); }

            //for connection creation dragging
            if (bHoverPin) {
                StartConnectingPin( iHoverIndex, bHoverPin );
                //SetCapture( hwnd );
                return 0;
            }

            //start of the dragging position
            if (iSelectedIndex >= 0) {
                iDragStartX = (short)LOWORD(lParam)  ; iDragStartY = (short)HIWORD(lParam);
                bDragging = 1; SetCapture( hwnd );
            }


            return 0;
        }
        case WM_LBUTTONUP: {       //Button released
            if (bDragging || bResizing || bConnectingPin ) { SetCapture( NULL ); }
            if (bDragging>1 || bResizing) { iMaxXIdx=-1 ; ScrollUpdate(hwnd,-1,-1); }
            if (bConnectingPin) { AddConnectingPin( iHoverIndex , bHoverPin ); }
            bDragging = 0; bResizing = 0;
            SetUpdate(); return 0;
        }
        case WM_KILLFOCUS:         //Lost Focus
        case WM_SETFOCUS: {        //Got Focus
          SetUpdate();
          return 0;
        }
        case WM_COMMAND: {         //Notification/Command from edit control
            int iID = LOWORD(wParam);         // control ID
            int iCode = HIWORD(wParam);      // notification code
            HWND hCtl = (HWND) lParam;      // handle of edit control
            if (hwndEdit && hCtl==hwndEdit) {
                if (iCode == EN_KILLFOCUS) {
                    //printf("ID=%i, code=%i\n",iID,iCode);
                    BeginEndEdit(iID,NULL);
                }
            }
            return 0;
        }
    }

    return DefWindowProc( hwnd ,message , wParam , lParam );
    #undef aObject_Content
    #undef SetUpdateAsync
    #undef SetUpdate
    #undef aObject
}

void Diagram_Init( HINSTANCE hinstance ) {
    // Setup window class
    WNDCLASS wcls = {
        .style         = 0, //CS_HREDRAW | CS_VREDRAW;
        .lpfnWndProc   = Diagram_WndProc,
        .cbClsExtra    = 0,
        .cbWndExtra    = 0,
        .hInstance     = hinstance,
        .hIcon         = NULL,
        .hCursor       = LoadCursor( NULL, IDC_ARROW ),
        .hbrBackground = NULL,
        .lpszMenuName  = NULL,
        .lpszClassName = "Diagram"
    };
    if ( !RegisterClass( &wcls ) ) { puts("Failed to register Diagram Control"); }
}

#undef _Grid2Pix
#undef _Pix2Grid
