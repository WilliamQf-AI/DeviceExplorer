#include "pch.h"
#include "DriversView.h"
#include <ClipboardHelper.h>
#include "Helpers.h"
#include <SortHelper.h>
#include <ToolbarHelper.h>

void CDriversView::UpdateUI(CUpdateUIBase& ui) {
}

void CDriversView::UpdateUI() {
	int selected = m_List.GetSelectedIndex();
	auto& item = m_Drivers[selected < 0 ? 0 : selected];
	UIEnable(ID_DRIVER_START, selected >= 0 && item.State == DriverState::Stopped);
	UIEnable(ID_DRIVER_STOP, selected >= 0 && item.State == DriverState::Running);
}

LRESULT CDriversView::OnStartStopDriver(WORD, WORD id, HWND, BOOL&) {
	int selected = m_List.GetSelectedIndex();
	ATLASSERT(selected >= 0);
	auto& item = m_Drivers[selected];
	struct Data {
		HWND hWnd;
		std::wstring Name;
		bool Start;
	};
	auto data = new Data{ m_hWnd, item.Name, id == ID_DRIVER_START };
	::TrySubmitThreadpoolCallback([](auto, auto param) {
		auto d = (Data*)param;
		auto ok = d->Start ? DriverManager::Start(d->Name) : DriverManager::Stop(d->Name);
		::SendMessage(d->hWnd, WM_UPDATEDRIVER, ok ? 0 : ::GetLastError(), reinterpret_cast<LPARAM>(d->Name.c_str()));
		delete d;
		}, data, nullptr);

	return 0;
}

LRESULT CDriversView::OnUpdateDriver(UINT, WPARAM wp, LPARAM lp, BOOL&) {
	if (wp) {
		AtlMessageBox(m_hWnd, L"Error encountered.", IDS_TITLE, MB_ICONERROR);
	}
	else {
		Refresh();
		UpdateUI();
	}
	return 0;
}

LRESULT CDriversView::OnCreate(UINT, WPARAM, LPARAM, BOOL&) {
	ToolBarButtonInfo buttons[] = {
		{ ID_DRIVER_START, IDI_GO, 0, L"Start" },
		{ ID_DRIVER_STOP, IDI_STOP, 0, L"Stop" },
	};
	CreateSimpleReBar(ATL_SIMPLE_REBAR_NOBORDER_STYLE);
	auto tb = ToolbarHelper::CreateAndInitToolBar(m_hWnd, buttons, _countof(buttons), 16);
	AddSimpleReBarBand(tb);
	UIAddToolBar(tb);
	CReBarCtrl rb(m_hWndToolBar);
	rb.LockBands(true);

	struct {
		UINT id, icon;
	} cmds[] = {
		{ ID_DRIVER_START, IDI_GO },
		{ ID_DRIVER_STOP, IDI_STOP },
	};
	for (auto& cmd : cmds) {
		AddCommand(cmd.id, AtlLoadIconImage(cmd.icon, 0, 16, 16));
	}
	UIAddMenu(IDR_CONTEXT);

	m_hWndClient = m_List.Create(m_hWnd, rcDefault, nullptr, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN
		| LVS_OWNERDATA | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_SINGLESEL);
	m_List.SetExtendedListViewStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_INFOTIP | LVS_EX_SUBITEMIMAGES);
	CImageList images;
	images.Create(16, 16, ILC_COLOR32 | ILC_MASK, 64, 32);
	UINT icons[] = { IDI_DRIVER, IDI_DRIVER_KMDF, IDI_DRIVER_UMDF, IDI_GO, IDI_STOP };
	for (auto icon : icons)
		images.AddIcon(AtlLoadIconImage(icon, 0, 16, 16));

	m_List.SetImageList(images, LVSIL_SMALL);

	auto cm = GetColumnManager(m_List);
	cm->AddColumn(L"Name", LVCFMT_LEFT, 150, ColumnType::Name);
	cm->AddColumn(L"Display Name", LVCFMT_LEFT, 250, ColumnType::DisplayName);
	cm->AddColumn(L"Type", LVCFMT_LEFT, 100, ColumnType::Type);
	cm->AddColumn(L"State", LVCFMT_LEFT, 80, ColumnType::State);
	cm->AddColumn(L"Start", LVCFMT_LEFT, 80, ColumnType::StartType);
	cm->AddColumn(L"Image Path", LVCFMT_LEFT, 350, ColumnType::ImagePath);
	cm->AddColumn(L"WDF Version", LVCFMT_RIGHT, 70, ColumnType::WDFVersion);

	auto pLoop = _Module.GetMessageLoop();
	pLoop->AddIdleHandler(this);

	Refresh();

	return 0;
}

LRESULT CDriversView::OnDestroy(UINT, WPARAM, LPARAM, BOOL&) {
	_Module.GetMessageLoop()->RemoveIdleHandler(this);
	return 0;
}

LRESULT CDriversView::OnSetFocus(UINT, WPARAM, LPARAM, BOOL&) {
	m_List.SetFocus();
	UpdateUI();

	return 0;
}

LRESULT CDriversView::OnCopy(WORD, WORD, HWND, BOOL&) {
	CString text;
	for (auto i = m_List.GetNextItem(-1, LVIS_SELECTED); i >= 0; i = m_List.GetNextItem(i, LVIS_SELECTED)) {
		text += ListViewHelper::GetRowAsString(m_List, i);
		text += L"\n";
	}
	ClipboardHelper::CopyText(m_hWnd, text.Left(text.GetLength() - 1));
	return 0;
}

LRESULT CDriversView::OnViewRefresh(WORD, WORD, HWND, BOOL&) {
	Refresh();
	return 0;
}

void CDriversView::OnStateChanged(HWND, int from, int to, UINT oldState, UINT newState) {
	if(newState & LVIS_SELECTED)
		UpdateUI();
}

bool CDriversView::OnRightClickList(HWND, int row, int col, POINT const& pt) {
	CMenu menu;
	menu.LoadMenu(IDR_CONTEXT);
	return ShowContextMenu(menu.GetSubMenu(2), 0, pt.x, pt.y);
}

void CDriversView::Refresh() {
	bool empty = m_Drivers.empty();
	m_Drivers.Set(DriverManager::EnumKernelDrivers());
	if (!empty)
		Sort(m_List);
	m_List.SetItemCountEx((int)m_Drivers.size(), LVSICF_NOSCROLL);
}

BOOL CDriversView::OnIdle() {
	UIUpdateToolBar();
	return FALSE;
}

CString CDriversView::GetColumnText(HWND, int row, int col) {
	auto& item = m_Drivers[row];

	switch (GetColumnManager(m_List)->GetColumnTag<ColumnType>(col)) {
		case ColumnType::Name: return item.Name.c_str();
		case ColumnType::DisplayName: return item.DisplayName.c_str();
		case ColumnType::ImagePath: return item.ImagePath.c_str();
		case ColumnType::State: return Helpers::DriverStateToString(item.State);
		case ColumnType::Type: return Helpers::DriverTypeToString(item.Type).c_str();
		case ColumnType::StartType: return Helpers::DriverStartTypeToString(item.StartType);
		case ColumnType::WDFVersion: return item.MajorVersion == 0 ? L"" : std::format(L"{}.{:02}", item.MajorVersion, item.MinorVersion).c_str();
	}
	return CString();
}

int CDriversView::GetRowImage(HWND h, int row, int col) {
	auto& item = m_Drivers[row];
	if (col == 0) {
		if ((item.Type & DeviceDriverType::KMDF) == DeviceDriverType::KMDF)
			return 1;
		if ((item.Type & DeviceDriverType::UMDF) == DeviceDriverType::UMDF)
			return 2;
		return 0;
	}
	if (GetColumnManager(h)->GetColumnTag<ColumnType>(col) == ColumnType::State)
		return item.State == DriverState::Running ? 3 :-1;
	return -1;
}

void CDriversView::DoSort(const SortInfo* si) {
	auto asc = si->SortAscending;
	auto col = GetColumnManager(si->hWnd)->GetColumnTag<ColumnType>(si->SortColumn);
	auto compare = [&](auto const& d1, auto const& d2) {
		switch (col) {
			case ColumnType::Name: return SortHelper::Sort(d1.Name, d2.Name, asc);
			case ColumnType::DisplayName: return SortHelper::Sort(d1.DisplayName, d2.DisplayName, asc);
			case ColumnType::StartType: return SortHelper::Sort(d1.StartType, d2.StartType, asc);
			case ColumnType::ImagePath: return SortHelper::Sort(d1.ImagePath, d2.ImagePath, asc);
			case ColumnType::State: return SortHelper::Sort(d1.State, d2.State, asc);
			case ColumnType::Type: return SortHelper::Sort(d1.Type, d2.Type, asc);
			case ColumnType::WDFVersion: return SortHelper::Sort(d1.MajorVersion * 100 + d1.MinorVersion, d2.MajorVersion * 100 + d2.MinorVersion, asc);
		}
		return false;
	};
	m_Drivers.Sort(compare);
}

int CDriversView::GetSaveColumnRange(HWND, int&) const {
	return 1;
}
