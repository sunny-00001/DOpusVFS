#pragma once

class CGifTlsData
{
protected:
	enum { IVTLSM_PLAY_FORWARDS = 0,
		   IVTLSM_PLAY_BACKWARDS,
		   IVTLSM_PAUSE,
		   IVTLSM_FLAT_FIT,
		   IVTLSM_FLAT_HORIZ,
		   IVTLSM_FLAT_VERT,
		   } m_mode;

	HWND	m_hWnd;

public:
	CGifTlsData()
		: m_hWnd(NULL)
		, m_mode(IVTLSM_PLAY_FORWARDS)
	{
	}

	inline void SetHWnd(HWND hWnd)			 { m_hWnd = hWnd; }
	inline HWND GetHWnd() const				 { return(m_hWnd); }

	inline bool IsPlayForwards() const		 { return( m_mode == IVTLSM_PLAY_FORWARDS  ); }
	inline bool IsPlayBackwards() const		 { return( m_mode == IVTLSM_PLAY_BACKWARDS ); }
	inline bool IsPause() const				 { return( m_mode == IVTLSM_PAUSE          ); }
	inline bool IsFlatten() const			 { return( m_mode == IVTLSM_FLAT_FIT || m_mode == IVTLSM_FLAT_HORIZ || m_mode == IVTLSM_FLAT_VERT); }
	inline bool IsFlattenFitToWindow() const { return( m_mode == IVTLSM_FLAT_FIT       ); }
	inline bool IsFlattenHorizontal() const	 { return( m_mode == IVTLSM_FLAT_HORIZ     ); }
	inline bool IsFlattenVertical() const	 { return( m_mode == IVTLSM_FLAT_VERT      ); }

	inline void SetPlayForwards()			 { m_mode = IVTLSM_PLAY_FORWARDS;  }
	inline void SetPlayBackwards()			 { m_mode = IVTLSM_PLAY_BACKWARDS; }
	inline void SetPause()					 { m_mode = IVTLSM_PAUSE;          }
	inline void SetFlattenFitToWindow()		 { m_mode = IVTLSM_FLAT_FIT;       }
	inline void SetFlattenHorizontal()		 { m_mode = IVTLSM_FLAT_HORIZ;     }
	inline void SetFlattenVertical()		 { m_mode = IVTLSM_FLAT_VERT;      }
};
