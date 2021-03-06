/*
** FamiTracker - NES/Famicom sound tracker
** Copyright (C) 2005-2014  Jonathan Liss
**
** 0CC-FamiTracker is (C) 2014-2018 HertzDevil
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** Library General Public License for more details.  To obtain a
** copy of the GNU Library General Public License, write to the Free
** Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** Any permitted reproduction of these routines, in whole or in part,
** must bear this legend.
*/

#include "BookmarkDlg.h"
#include <string>
#include "FamiTrackerDoc.h"
#include "FamiTrackerModule.h"
#include "FamiTrackerView.h"
#include "SongData.h"
#include "SongView.h"
#include "MainFrm.h"
#include "Bookmark.h"
#include "BookmarkCollection.h"
#include "str_conv/str_conv.hpp"



// CListBoxEx

void CListBoxEx::DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct) {
	ASSERT(lpDrawItemStruct->CtlType == ODT_LISTBOX);
	if (lpDrawItemStruct->itemID == -1)
		return;

	auto parent = dynamic_cast<CBookmarkDlg *>(GetParent());
	bool active = parent && parent->IsBookmarkValid(lpDrawItemStruct->itemID);

	CDC dc;
	dc.Attach(lpDrawItemStruct->hDC);
	COLORREF crOldTextColor = dc.GetTextColor();
	COLORREF crOldBkColor = dc.GetBkColor();

	CStringW str;
	GetText(lpDrawItemStruct->itemID, str);

	if ((lpDrawItemStruct->itemAction | ODA_SELECT) &&
		(lpDrawItemStruct->itemState & ODS_SELECTED)) {
		dc.SetTextColor(::GetSysColor(COLOR_HIGHLIGHTTEXT));
		dc.SetBkColor(::GetSysColor(COLOR_HIGHLIGHT));
		dc.FillSolidRect(&lpDrawItemStruct->rcItem,
			::GetSysColor(COLOR_HIGHLIGHT));
	}
	else
		dc.FillSolidRect(&lpDrawItemStruct->rcItem, crOldBkColor);

	if (!active)
		dc.SetTextColor(::GetSysColor(COLOR_GRAYTEXT));

	dc.SetWindowOrg(-2, 0);
	dc.DrawTextW(str, (int)str.GetLength(), &lpDrawItemStruct->rcItem, DT_SINGLELINE);

	dc.SetWindowOrg(0, 0);
	dc.SetTextColor(crOldTextColor);
	dc.SetBkColor(crOldBkColor);

	if ((lpDrawItemStruct->itemAction | ODA_FOCUS) &&
		(lpDrawItemStruct->itemState & ODS_FOCUS))
		dc.DrawFocusRect(&lpDrawItemStruct->rcItem);

	dc.Detach();
}

void CListBoxEx::MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct) {
	ASSERT(lpMeasureItemStruct->CtlType == ODT_LISTBOX);
	CDC *pDC = GetDC();

	TEXTMETRIC tm;
	pDC->GetTextMetricsW(&tm);
	lpMeasureItemStruct->itemHeight = tm.tmHeight * 3 / 4;

	ReleaseDC(pDC);
}



// CBookmarkDlg dialog

IMPLEMENT_DYNAMIC(CBookmarkDlg, CDialog)

CBookmarkDlg::CBookmarkDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CBookmarkDlg::IDD, pParent)
{

}

void CBookmarkDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}


BEGIN_MESSAGE_MAP(CBookmarkDlg, CDialog)
	ON_BN_CLICKED(IDC_BUTTON_BOOKMARK_ADD, OnBnClickedButtonBookmarkAdd)
	ON_BN_CLICKED(IDC_BUTTON_BOOKMARK_UPDATE, OnBnClickedButtonBookmarkUpdate)
	ON_BN_CLICKED(IDC_BUTTON_BOOKMARK_REMOVE, OnBnClickedButtonBookmarkRemove)
	ON_BN_CLICKED(IDC_BUTTON_BOOKMARK_MOVEUP, OnBnClickedButtonBookmarkMoveup)
	ON_BN_CLICKED(IDC_BUTTON_BOOKMARK_MOVEDOWN, OnBnClickedButtonBookmarkMovedown)
	ON_BN_CLICKED(IDC_BUTTON_BOOKMARK_CLEARALL, OnBnClickedButtonBookmarkClearall)
	ON_BN_CLICKED(IDC_BUTTON_BOOKMARK_SORTP, OnBnClickedButtonBookmarkSortp)
	ON_BN_CLICKED(IDC_BUTTON_BOOKMARK_SORTN, OnBnClickedButtonBookmarkSortn)
	ON_LBN_SELCHANGE(IDC_LIST_BOOKMARKS, OnLbnSelchangeListBookmarks)
	ON_LBN_DBLCLK(IDC_LIST_BOOKMARKS, OnLbnDblclkListBookmarks)
	ON_BN_CLICKED(IDC_CHECK_BOOKMARK_HIGH1, OnBnClickedCheckBookmarkHigh1)
	ON_BN_CLICKED(IDC_CHECK_BOOKMARK_HIGH2, OnBnClickedCheckBookmarkHigh2)
	ON_BN_CLICKED(IDC_CHECK_BOOKMARK_PERSIST, OnBnClickedCheckBookmarkPersist)
END_MESSAGE_MAP()


// CBookmarkDlg message handlers

std::unique_ptr<CBookmark> CBookmarkDlg::MakeBookmark() const
{
	CStringW str;
	GetDlgItem(IDC_EDIT_BOOKMARK_NAME)->GetWindowTextW(str);

	auto pMark = std::make_unique<CBookmark>(m_cSpinFrame.GetPos(), m_cSpinRow.GetPos());
	pMark->m_Highlight.First = m_bEnableHighlight1 ? m_cSpinHighlight1.GetPos() : -1;
	pMark->m_Highlight.Second = m_bEnableHighlight2 ? m_cSpinHighlight2.GetPos() : -1;
	pMark->m_Highlight.Offset = 0;
	pMark->m_bPersist = m_bPersist;
	pMark->m_sName = conv::to_utf8(str);

	if (pMark->m_iFrame >= MAX_FRAMES)
		pMark->m_iFrame = MAX_FRAMES - 1;
	if (pMark->m_iRow >= MAX_PATTERN_LENGTH)
		pMark->m_iRow = MAX_PATTERN_LENGTH - 1;

	return pMark;
}

void CBookmarkDlg::UpdateBookmarkList()
{
	LoadBookmarks(m_iTrack);
	m_pDocument->UpdateAllViews(NULL, UPDATE_PATTERN);
	m_pDocument->UpdateAllViews(NULL, UPDATE_FRAME);
}

void CBookmarkDlg::LoadBookmarks(int Track)
{
	m_cListBookmark.ResetContent();
	m_iTrack = Track;
	m_pCollection = &m_pDocument->GetModule()->GetSong(Track)->GetBookmarks();

	for (unsigned i = 0; i < m_pCollection->GetCount(); ++i) {
		const CBookmark *pMark = m_pCollection->GetBookmark(i);
		CStringW str = conv::to_wide(pMark->m_sName).data();
		if (str.IsEmpty())
			str = L"Bookmark";
		AppendFormatW(str, L" (%02X,%02X)", pMark->m_iFrame, pMark->m_iRow);
		m_cListBookmark.AddString(str);
	}
}

void CBookmarkDlg::SelectBookmark(int Pos)
{
	int n = m_cListBookmark.SetCurSel(Pos);
	if (n == LB_ERR)
		m_cListBookmark.SetCurSel(-1);
	OnLbnSelchangeListBookmarks();
}


bool CBookmarkDlg::IsBookmarkValid(unsigned index) const {
	if (m_pCollection && m_pDocument)
		if (auto pBookmark = m_pCollection->GetBookmark(index)) {
			auto *pSongView = CFamiTrackerView::GetView()->GetSongView();
			return pBookmark->m_iFrame < pSongView->GetSong().GetFrameCount() &&
				pBookmark->m_iRow < (unsigned)pSongView->GetFrameLength(pBookmark->m_iFrame);
		}
	return false;
}

BOOL CBookmarkDlg::OnInitDialog()
{
	m_cListBookmark.SubclassDlgItem(IDC_LIST_BOOKMARKS, this);
	m_cSpinFrame.SubclassDlgItem(IDC_SPIN_BOOKMARK_FRAME, this);
	m_cSpinRow.SubclassDlgItem(IDC_SPIN_BOOKMARK_ROW, this);
	m_cSpinHighlight1.SubclassDlgItem(IDC_SPIN_BOOKMARK_HIGH1, this);
	m_cSpinHighlight2.SubclassDlgItem(IDC_SPIN_BOOKMARK_HIGH2, this);

	m_pDocument = CFamiTrackerDoc::GetDoc();
	m_iTrack = 0U;

	m_cSpinFrame.SetRange(0, MAX_FRAMES - 1);
	m_cSpinRow.SetRange(0, MAX_PATTERN_LENGTH - 1);
	m_cSpinHighlight1.SetRange(0, MAX_PATTERN_LENGTH);
	m_cSpinHighlight2.SetRange(0, MAX_PATTERN_LENGTH);

	m_cListBookmark.SetCurSel(-1);

	m_bEnableHighlight1 = false;
	m_bEnableHighlight2 = false;
	m_bPersist = false;

	static_cast<CEdit*>(GetDlgItem(IDC_EDIT_BOOKMARK_FRAME))->SetLimitText(3);
	static_cast<CEdit*>(GetDlgItem(IDC_EDIT_BOOKMARK_ROW))->SetLimitText(3);
	static_cast<CEdit*>(GetDlgItem(IDC_EDIT_BOOKMARK_HIGH1))->SetLimitText(3);
	static_cast<CEdit*>(GetDlgItem(IDC_EDIT_BOOKMARK_HIGH2))->SetLimitText(3);

	return CDialog::OnInitDialog();
}

BOOL CBookmarkDlg::PreTranslateMessage(MSG* pMsg)
{
	if (GetFocus() == &m_cListBookmark) {
		if (pMsg->message == WM_KEYDOWN) {
			switch (pMsg->wParam) {
				case VK_INSERT:
					OnBnClickedButtonBookmarkAdd();
					break;
				case VK_DELETE:
					OnBnClickedButtonBookmarkRemove();
					break;
				case VK_UP:
					if ((::GetKeyState(VK_CONTROL) & 0x80) == 0x80) {
						OnBnClickedButtonBookmarkMoveup();
						return TRUE;
					}
					break;
				case VK_DOWN:
					if ((::GetKeyState(VK_CONTROL) & 0x80) == 0x80) {
						OnBnClickedButtonBookmarkMovedown();
						return TRUE;
					}
					break;
			}
		}
	}

	if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN) return TRUE;

	return CDialog::PreTranslateMessage(pMsg);
}

void CBookmarkDlg::OnBnClickedButtonBookmarkAdd()
{
	if (m_pCollection->AddBookmark(MakeBookmark()))
		m_pDocument->ModifyIrreversible();
	UpdateBookmarkList();
	m_cListBookmark.SetCurSel(m_pCollection->GetCount() - 1);
}

void CBookmarkDlg::OnBnClickedButtonBookmarkUpdate()
{
	int pos = m_cListBookmark.GetCurSel();
	if (pos == LB_ERR) return;

	if (m_pCollection->SetBookmark(pos, MakeBookmark()))
		m_pDocument->ModifyIrreversible();
	UpdateBookmarkList();
	m_cListBookmark.SetCurSel(pos);
}

void CBookmarkDlg::OnBnClickedButtonBookmarkRemove()
{
	int pos = m_cListBookmark.GetCurSel();
	if (pos != LB_ERR) {
		if (m_pCollection->RemoveBookmark(pos))
			m_pDocument->ModifyIrreversible();
		UpdateBookmarkList();
		if (int Count = m_pCollection->GetCount()) {
			if (pos == Count) --pos;
			m_cListBookmark.SetCurSel(pos);
		}
		OnLbnSelchangeListBookmarks();
	}
	m_cListBookmark.SetFocus();
}

void CBookmarkDlg::OnBnClickedButtonBookmarkMoveup()
{
	int pos = m_cListBookmark.GetCurSel();
	if (pos != LB_ERR && pos != 0) {
		if (m_pCollection->SwapBookmarks(pos, pos - 1))
			m_pDocument->ModifyIrreversible();
		UpdateBookmarkList();
		m_cListBookmark.SetCurSel(pos - 1);
	}
	m_cListBookmark.SetFocus();
}

void CBookmarkDlg::OnBnClickedButtonBookmarkMovedown()
{
	int pos = m_cListBookmark.GetCurSel();
	if (pos != LB_ERR && (unsigned)pos != m_pCollection->GetCount() - 1) {
		if (m_pCollection->SwapBookmarks(pos, pos + 1))
			m_pDocument->ModifyIrreversible();
		UpdateBookmarkList();
		m_cListBookmark.SetCurSel(pos + 1);
	}
	m_cListBookmark.SetFocus();
}

void CBookmarkDlg::OnBnClickedButtonBookmarkClearall()
{
	if (m_pCollection->ClearBookmarks())
		m_pDocument->ModifyIrreversible();
	UpdateBookmarkList();
}

void CBookmarkDlg::OnBnClickedButtonBookmarkSortp()
{
	if (m_pCollection->GetCount()) {
		const CBookmark *pMark = m_pCollection->GetBookmark(m_cListBookmark.GetCurSel());
		if (m_pCollection->SortByPosition(false))
			m_pDocument->ModifyIrreversible();
		UpdateBookmarkList();
		m_cListBookmark.SetCurSel(m_pCollection->GetBookmarkIndex(pMark));
	}
	m_cListBookmark.SetFocus();
}

void CBookmarkDlg::OnBnClickedButtonBookmarkSortn()
{
	if (m_pCollection->GetCount()) {
		const CBookmark *pMark = m_pCollection->GetBookmark(m_cListBookmark.GetCurSel());
		if (m_pCollection->SortByName(false))
			m_pDocument->ModifyIrreversible();
		UpdateBookmarkList();
		m_cListBookmark.SetCurSel(m_pCollection->GetBookmarkIndex(pMark));
	}
	m_cListBookmark.SetFocus();
}

void CBookmarkDlg::OnLbnSelchangeListBookmarks()
{
	int pos = m_cListBookmark.GetCurSel();
	if (pos == LB_ERR) return;

	const CBookmark *pMark = m_pCollection->GetBookmark(pos);
	m_bPersist = pMark->m_bPersist;
	static_cast<CButton*>(GetDlgItem(IDC_CHECK_BOOKMARK_PERSIST))->SetCheck(m_bPersist ? BST_CHECKED : BST_UNCHECKED);
	GetDlgItem(IDC_EDIT_BOOKMARK_NAME)->SetWindowTextW(conv::to_wide(pMark->m_sName).data());

	const auto &hl = m_pDocument->GetModule()->GetSong(m_iTrack)->GetRowHighlight();

	m_bEnableHighlight1 = pMark->m_Highlight.First != -1;
	m_cSpinHighlight1.SetPos(m_bEnableHighlight1 ? pMark->m_Highlight.First : hl.First);
	static_cast<CButton*>(GetDlgItem(IDC_CHECK_BOOKMARK_HIGH1))->SetCheck(m_bEnableHighlight1);
	static_cast<CEdit*>(GetDlgItem(IDC_EDIT_BOOKMARK_HIGH1))->EnableWindow(m_bEnableHighlight1);

	m_bEnableHighlight2 = pMark->m_Highlight.Second != -1;
	m_cSpinHighlight2.SetPos(m_bEnableHighlight2 ? pMark->m_Highlight.Second : hl.Second);
	static_cast<CButton*>(GetDlgItem(IDC_CHECK_BOOKMARK_HIGH2))->SetCheck(m_bEnableHighlight2);
	static_cast<CEdit*>(GetDlgItem(IDC_EDIT_BOOKMARK_HIGH2))->EnableWindow(m_bEnableHighlight2);

	static_cast<CButton*>(GetDlgItem(IDC_CHECK_BOOKMARK_PERSIST))->SetCheck(m_bPersist);

	m_cSpinFrame.SetPos(pMark->m_iFrame);
	m_cSpinRow.SetPos(pMark->m_iRow);
}

void CBookmarkDlg::OnLbnDblclkListBookmarks()
{
	CPoint cursor;
	cursor.x = GetCurrentMessage()->pt.x;
	cursor.y = GetCurrentMessage()->pt.y;

	m_cListBookmark.ScreenToClient(&cursor);

	BOOL is_outside = FALSE;
	UINT item_index = m_cListBookmark.ItemFromPoint(cursor, is_outside);
	if (!is_outside && IsBookmarkValid(item_index)) {
		auto *pView = CFamiTrackerView::GetView();
		CBookmark *pMark = m_pCollection->GetBookmark(item_index);
		pView->SelectFrame(pMark->m_iFrame);
		pView->SelectRow(pMark->m_iRow);
		//static_cast<CMainFrame*>(AfxGetMainWnd())->SelectTrack(pMark->m_iTrack);
		pView->SetFocus();
	}
}

void CBookmarkDlg::OnBnClickedCheckBookmarkHigh1()
{
	m_bEnableHighlight1 = static_cast<CButton*>(GetDlgItem(IDC_CHECK_BOOKMARK_HIGH1))->GetCheck() == BST_CHECKED;
	static_cast<CEdit*>(GetDlgItem(IDC_EDIT_BOOKMARK_HIGH1))->EnableWindow(m_bEnableHighlight1);
}

void CBookmarkDlg::OnBnClickedCheckBookmarkHigh2()
{
	m_bEnableHighlight2 = static_cast<CButton*>(GetDlgItem(IDC_CHECK_BOOKMARK_HIGH2))->GetCheck() == BST_CHECKED;
	static_cast<CEdit*>(GetDlgItem(IDC_EDIT_BOOKMARK_HIGH2))->EnableWindow(m_bEnableHighlight2);
}

void CBookmarkDlg::OnBnClickedCheckBookmarkPersist()
{
	m_bPersist = static_cast<CButton*>(GetDlgItem(IDC_CHECK_BOOKMARK_PERSIST))->GetCheck() == BST_CHECKED;
}
