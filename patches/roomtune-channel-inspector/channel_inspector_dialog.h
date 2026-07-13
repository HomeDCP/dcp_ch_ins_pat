/*
    RoomTune — DCP 검수용 오디오 채널 인스펙터 다이얼로그 (비모달 UI)
    DCP-o-matic 파생물 (GPL-2.0-or-later).

    채널별 solo/mute 체크박스 + 실시간 피크 미터(dBFS). 자체 오디오 상태는
    없고 모든 제어를 FilmViewer 포워더로 위임한다. 열릴 때 인스펙터를 활성화
    (지연 identity Butler 재구성)하고 닫힐 때 비활성화한다.
*/

#ifndef DCPOMATIC_CHANNEL_INSPECTOR_DIALOG_H
#define DCPOMATIC_CHANNEL_INSPECTOR_DIALOG_H

#include "film_viewer.h"
#include "wx_util.h"
#include "lib/film.h"
#include "lib/util.h"
#include <dcp/warnings.h>
LIBDCP_DISABLE_WARNINGS
#include <wx/wx.h>
LIBDCP_ENABLE_WARNINGS
#include <algorithm>
#include <list>
#include <vector>

/* wxDialog 가 아니라 wxFrame 을 쓴다: macOS wxDialog 는 자식 컨트롤(체크박스)의
   command 이벤트를 취소(close)로 오인해 다이얼로그를 간헐적으로 닫아버린다.
   wxFrame 은 이 특수 동작이 없어 solo/mute 클릭이 창을 닫지 않는다. */
class ChannelInspectorDialog : public wxFrame
{
public:
	ChannelInspectorDialog(wxWindow* parent, FilmViewer& viewer)
		: wxFrame(parent, wxID_ANY, _("Channel inspector"), wxDefaultPosition, wxDefaultSize,
		          wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT)
		, _viewer(viewer)
		, _timer(this)
	{
		_sizer = new wxBoxSizer(wxVERTICAL);

		auto intro = new wxStaticText(this, wxID_ANY,
			_("Solo/mute individual DCP channels and watch per-channel peak levels to verify mapping."));
		_sizer->Add(intro, 0, wxALL, DCPOMATIC_DIALOG_BORDER);

		_grid = new wxFlexGridSizer(4, 4, 12);   /* 4열: Label / Solo / Mute / Peak */
		_sizer->Add(_grid, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, DCPOMATIC_DIALOG_BORDER);

		auto clear = new wxButton(this, wxID_ANY, _("Clear solo/mute"));
		clear->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { clear_clicked(); });
		_sizer->Add(clear, 0, wxLEFT | wxRIGHT | wxBOTTOM, DCPOMATIC_DIALOG_BORDER);

		SetSizerAndFit(_sizer);

		Bind(wxEVT_TIMER, [this](wxTimerEvent&) { on_timer(); });
		Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& ev) {
			_timer.Stop();
			_viewer.inspector_set_active(false);
			Hide();
			ev.Veto();   /* wx_ptr 이 재사용하므로 파괴하지 않고 숨김 */
		});
	}

	/** 다이얼로그를 연다(메뉴에서 호출). Show() 를 override 하면 wxWidgets 의 내부
	    Show/Hide 호출(레이아웃·포커스)마다 인스펙터가 토글되어 자멸하므로, override
	    대신 명시적 진입점을 둔다. */
	void open()
	{
		_viewer.inspector_set_active(true);
		rebuild_rows();
		_timer.Start(100);
		wxFrame::Show(true);
		Raise();
	}

private:
	void rebuild_rows()
	{
		_grid->Clear(true);
		_solo.clear();
		_mute.clear();
		_peak.clear();

		std::list<int> mapped;
		if (auto film = _viewer.film()) {
			mapped = film->mapped_audio_channels();
		}

		int const n = _viewer.inspector_dcp_channels();
		for (int c = 0; c < n; ++c) {
			bool const is_mapped = std::find(mapped.begin(), mapped.end(), c) != mapped.end();
			auto lab = wxString::Format(wxT("%d  "), c) + std_to_wx(short_audio_channel_name(c));
			if (!is_mapped) {
				lab += wxT("  (unmapped)");
			}
			auto label = new wxStaticText(this, wxID_ANY, lab);
			if (!is_mapped) {
				label->Enable(false);   /* 회색 처리 */
			}
			auto solo = new wxCheckBox(this, wxID_ANY, _("Solo"));
			auto mute = new wxCheckBox(this, wxID_ANY, _("Mute"));
			auto peak = new wxStaticText(this, wxID_ANY, wxT("-inf dB"));
			solo->Bind(wxEVT_CHECKBOX, [this, c](wxCommandEvent& ev) { _viewer.inspector_set_solo(c, ev.IsChecked()); });
			mute->Bind(wxEVT_CHECKBOX, [this, c](wxCommandEvent& ev) { _viewer.inspector_set_mute(c, ev.IsChecked()); });

			_grid->Add(label, 0, wxALIGN_CENTER_VERTICAL);
			_grid->Add(solo, 0);
			_grid->Add(mute, 0);
			_grid->Add(peak, 0, wxALIGN_CENTER_VERTICAL);
			_solo.push_back(solo);
			_mute.push_back(mute);
			_peak.push_back(peak);
		}

		_sizer->Layout();
		Fit();
	}

	void clear_clicked()
	{
		_viewer.inspector_clear();
		for (auto b: _solo) {
			b->SetValue(false);
		}
		for (auto b: _mute) {
			b->SetValue(false);
		}
	}

	void on_timer()
	{
		for (size_t c = 0; c < _peak.size(); ++c) {
			float const db = _viewer.inspector_peak_dbfs(static_cast<int>(c));
			_peak[c]->SetLabel(db <= -119.0f ? wxString(wxT("-inf dB")) : wxString::Format(wxT("%.1f dB"), db));
		}
	}

	FilmViewer& _viewer;
	wxTimer _timer;
	wxBoxSizer* _sizer = nullptr;
	wxFlexGridSizer* _grid = nullptr;
	std::vector<wxCheckBox*> _solo;
	std::vector<wxCheckBox*> _mute;
	std::vector<wxStaticText*> _peak;
};

#endif
