// list control inline functions
inline LRESULT lbAddString(int iID, LPCTSTR pzText, LPARAM dwData) {
    LRESULT iResu = SendMessage(_CTL(iID), LB_ADDSTRING, 0, (LPARAM)pzText);
    if (iResu != LB_ERR && iResu != LB_ERRSPACE) {
        SendMessage(_CTL(iID), LB_SETITEMDATA, (WPARAM)iResu, (LPARAM)dwData);
    }
    return iResu;
}

// edit control inline functions
inline void emSetLimitText(int iID, int iLimit) {
    SendMessage(_CTL(iID), EM_SETLIMITTEXT, (WPARAM)iLimit, 0);
}

// combobox control inline functions
inline LRESULT cbAddString(int iID, LPCTSTR pzText, LPARAM dwData) {
    LRESULT iResu = SendMessage(_CTL(iID), CB_ADDSTRING, 0, (LPARAM)pzText);
    if (iResu != CB_ERR && iResu != CB_ERRSPACE) {
        SendMessage(_CTL(iID), CB_SETITEMDATA, (WPARAM)iResu, (LPARAM)dwData);
    }
    return iResu;
}
inline LRESULT cbSetCurSel(int iID, int iIndex) {
    SendMessage(_CTL(iID), CB_SETCURSEL, (WPARAM)iIndex, 0);
}
inline LRESULT cbGetCurSel(int iID) {
    return SendMessage(_CTL(iID), CB_GETCURSEL, 0, 0);
}
inline LRESULT cbSetEditSel( int iID, int ichStart, int ichEnd ) {
    return SendMessage(_CTL(iID), CB_SETEDITSEL  , 0 , MAKELPARAM( ichStart, ichEnd ) );
}
inline LRESULT cbSelectString( int iID , int iIdxStart , LPCTSTR pzText) {
    return SendMessage(_CTL(iID), CB_SELECTSTRING , iIdxStart , (LPARAM)pzText);
}
inline LRESULT cbGetItemData(int iID, int iIndex) {
    return SendMessage(_CTL(iID), CB_GETITEMDATA, (WPARAM)iIndex, 0);
}

// treeview control inline functions
#define FindTreeItemByLParam( _iID , _targetLParam ) FindTreeItemByLParamEx( _iID, _targetLParam, NULL )
HTREEITEM FindTreeItemByLParamEx(int iID, LPARAM targetLParam, HTREEITEM hStart /* = NULL */) {
    //if (iID == 0) { return NULL; }
    // Default to root if caller passed NULL
    if (hStart == NULL) { hStart = TreeView_GetRoot(_CTL(iID)); }

    HTREEITEM hItem = hStart;
    while (hItem != NULL) {
        TVITEM tvi = {0};
        tvi.mask = TVIF_HANDLE | TVIF_PARAM;
        tvi.hItem = hItem;
        if (TreeView_GetItem( _CTL(iID), &tvi) && tvi.lParam == targetLParam) { return hItem; } // Match found
        // Recursively search child items
        HTREEITEM hChild = TreeView_GetChild(_CTL(iID), hItem);
        if (hChild != NULL) {
            HTREEITEM hFound = FindTreeItemByLParamEx( iID , targetLParam, hChild);
            if (hFound != NULL) { return hFound; }
        }
        // Move to sibling at current level
        hItem = TreeView_GetNextSibling(_CTL(iID), hItem);
    }

    return NULL;
}

LRESULT tvSelectItem(int iID, HTREEITEM hItem) {
    //make sure item is visible and parent nodes are expanded
    return TreeView_SelectItem( _CTL(iID) , hItem );
    while (hItem != NULL) {
        printf("Item=%p\n", hItem);
        TreeView_Expand( _CTL(iID), TVE_EXPAND , hItem );
        TreeView_EnsureVisible( _CTL(iID), hItem );
        hItem = TreeView_GetParent(_CTL(iID), hItem);
    }
}

inline LRESULT ctlParentCommand(int iID, int iCode) {
    return SendMessage( GetParent(_CTL(iID)), WM_COMMAND, MAKEWPARAM(iID,iCode), (LPARAM)_CTL(iID));
}
inline LRESULT ctlParentCommandPost(int iID, int iCode) {
    return PostMessage( GetParent(_CTL(iID)), WM_COMMAND, MAKEWPARAM(iID,iCode), (LPARAM)_CTL(iID));
}
