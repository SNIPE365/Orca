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
inline LRESULT cbGetItemData(int iID, int iIndex) {
    return SendMessage(_CTL(iID), CB_GETITEMDATA, (WPARAM)iIndex, 0);
}

inline LRESULT ctlParentCommand(int iID, int iCode) {
    return SendMessage( GetParent(_CTL(iID)), WM_COMMAND, MAKEWPARAM(iID,iCode), (LPARAM)_CTL(iID));
}
