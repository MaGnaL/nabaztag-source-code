/*
	Ralink RT2501 driver for Violet embedded platforms
	(c) 2006 Sebastien Bourdeauducq
*/

#ifndef _IEEE80211_H_
#define _IEEE80211_H_

#pragma pack(1)

#define IEEE80211_ADDR_LEN	6
#define IEEE80211_OUI_LEN	4
#define IEEE80211_SSID_MAXLEN	32

#define IEEE80211_IS_MULTICAST(addr) (((char *)(addr))[0] & 0x01)

struct ieee80211_frame {
	unsigned char	i_fc[2];
	unsigned char	i_dur[2];
	unsigned char	i_addr1[IEEE80211_ADDR_LEN];
	unsigned char	i_addr2[IEEE80211_ADDR_LEN];
	unsigned char	i_addr3[IEEE80211_ADDR_LEN];
	unsigned char	i_seq[2];
	/* possibly followed by addr4 */
};

struct ieee80211_frame_addr4 {
	unsigned char	i_fc[2];
	unsigned char	i_dur[2];
	unsigned char	i_addr1[IEEE80211_ADDR_LEN];
	unsigned char	i_addr2[IEEE80211_ADDR_LEN];
	unsigned char	i_addr3[IEEE80211_ADDR_LEN];
	unsigned char	i_seq[2];
	unsigned char	i_addr4[IEEE80211_ADDR_LEN];
};

#define IEEE80211_FC0_VERSION_MASK		0x03
#define IEEE80211_FC0_VERSION_SHIFT		0
#define IEEE80211_FC0_VERSION_0			0x00
#define IEEE80211_FC0_TYPE_MASK			0x0c
#define IEEE80211_FC0_TYPE_SHIFT		2
#define IEEE80211_FC0_TYPE_MGT			0x00
#define IEEE80211_FC0_TYPE_CTL			0x04
#define IEEE80211_FC0_TYPE_DATA			0x08

#define IEEE80211_FC0_SUBTYPE_MASK		0xf0
#define IEEE80211_FC0_SUBTYPE_SHIFT		4
/* for TYPE_MGT */
#define IEEE80211_FC0_SUBTYPE_ASSOC_REQ		0x00
#define IEEE80211_FC0_SUBTYPE_ASSOC_RESP	0x10
#define IEEE80211_FC0_SUBTYPE_REASSOC_REQ	0x20
#define IEEE80211_FC0_SUBTYPE_REASSOC_RESP	0x30
#define IEEE80211_FC0_SUBTYPE_PROBE_REQ		0x40
#define IEEE80211_FC0_SUBTYPE_PROBE_RESP	0x50
#define IEEE80211_FC0_SUBTYPE_BEACON		0x80
#define IEEE80211_FC0_SUBTYPE_ATIM		0x90
#define IEEE80211_FC0_SUBTYPE_DISASSOC		0xa0
#define IEEE80211_FC0_SUBTYPE_AUTH		0xb0
#define IEEE80211_FC0_SUBTYPE_DEAUTH		0xc0
/* for TYPE_CTL */
#define IEEE80211_FC0_SUBTYPE_PS_POLL		0xa0
#define IEEE80211_FC0_SUBTYPE_RTS		0xb0
#define IEEE80211_FC0_SUBTYPE_CTS		0xc0
#define IEEE80211_FC0_SUBTYPE_ACK		0xd0
#define IEEE80211_FC0_SUBTYPE_CF_END		0xe0
#define IEEE80211_FC0_SUBTYPE_CF_END_ACK	0xf0
/* for TYPE_DATA (bit combination) */
#define IEEE80211_FC0_SUBTYPE_DATA		0x00
#define IEEE80211_FC0_SUBTYPE_CF_ACK		0x10
#define IEEE80211_FC0_SUBTYPE_CF_POLL		0x20
#define IEEE80211_FC0_SUBTYPE_CF_ACPL		0x30
#define IEEE80211_FC0_SUBTYPE_NODATA		0x40
#define IEEE80211_FC0_SUBTYPE_CFACK		0x50
#define IEEE80211_FC0_SUBTYPE_CFPOLL		0x60
#define IEEE80211_FC0_SUBTYPE_CF_ACK_CF_ACK	0x70
#define IEEE80211_FC0_SUBTYPE_QOS		0x80
#define IEEE80211_FC0_SUBTYPE_QOS_NULL		0xc0

#define IEEE80211_FC1_DIR_MASK			0x03
#define IEEE80211_FC1_DIR_NODS			0x00	/* STA->STA      */
#define IEEE80211_FC1_DIR_TODS			0x01	/* STA->AP       */
#define IEEE80211_FC1_DIR_FROMDS		0x02	/* AP ->STA      */
#define IEEE80211_FC1_DIR_DSTODS		0x03	/* AP ->AP (WDS) */

#define IEEE80211_FC1_MORE_FRAG			0x04
#define IEEE80211_FC1_RETRY			0x08
#define IEEE80211_FC1_PWR_MGT			0x10
#define IEEE80211_FC1_MORE_DATA			0x20
#define IEEE80211_FC1_PROTECTED			0x40
#define IEEE80211_FC1_ORDER			0x80

#define IEEE80211_SEQ_FRAG_MASK			0x000f
#define IEEE80211_SEQ_FRAG_SHIFT		0
#define IEEE80211_SEQ_SEQ_MASK			0xfff0
#define IEEE80211_SEQ_SEQ_SHIFT			4

#define IEEE80211_NWID_LEN			32

#define IEEE80211_QOS_TXOP			0x00ff
/* bit 8 is reserved */
#define IEEE80211_QOS_ACKPOLICY			0x60
#define IEEE80211_QOS_ACKPOLICY_S		5
#define IEEE80211_QOS_ESOP			0x10
#define IEEE80211_QOS_ESOP_S			4
#define IEEE80211_QOS_TID			0x0f

/* Control frames */

struct ieee80211_frame_min {
	unsigned char	i_fc[2];
	unsigned char	i_dur[2];
	unsigned char	i_addr1[IEEE80211_ADDR_LEN];
	unsigned char	i_addr2[IEEE80211_ADDR_LEN];
};

struct ieee80211_frame_rts {
	unsigned char	i_fc[2];
	unsigned char	i_dur[2];
	unsigned char	i_ra[IEEE80211_ADDR_LEN];
	unsigned char	i_ta[IEEE80211_ADDR_LEN];
};

struct ieee80211_frame_cts {
	unsigned char	i_fc[2];
	unsigned char	i_dur[2];
	unsigned char	i_ra[IEEE80211_ADDR_LEN];
};

struct ieee80211_frame_ack {
	unsigned char	i_fc[2];
	unsigned char	i_dur[2];
	unsigned char	i_ra[IEEE80211_ADDR_LEN];
};

struct ieee80211_frame_pspoll {
	unsigned char	i_fc[2];
	unsigned char	i_aid[2];
	unsigned char	i_bssid[IEEE80211_ADDR_LEN];
	unsigned char	i_ta[IEEE80211_ADDR_LEN];
};

struct ieee80211_frame_cfend {		/* NB: also CF-End+CF-Ack */
	unsigned char	i_fc[2];
	unsigned char	i_dur[2];	/* should be zero */
	unsigned char	i_ra[IEEE80211_ADDR_LEN];
	unsigned char	i_bssid[IEEE80211_ADDR_LEN];
};

#define IEEE80211_CAPINFO_ESS			0x0001
#define IEEE80211_CAPINFO_IBSS			0x0002
#define IEEE80211_CAPINFO_CF_POLLABLE		0x0004
#define IEEE80211_CAPINFO_CF_POLLREQ		0x0008
#define IEEE80211_CAPINFO_PRIVACY		0x0010
#define IEEE80211_CAPINFO_SHORT_PREAMBLE	0x0020
#define IEEE80211_CAPINFO_PBCC			0x0040
#define IEEE80211_CAPINFO_CHNL_AGILITY		0x0080
/* bits 8-9 are reserved */
#define IEEE80211_CAPINFO_SHORT_SLOTTIME	0x0400
#define IEEE80211_CAPINFO_RSN			0x0800
/* bit 12 is reserved */
#define IEEE80211_CAPINFO_DSSSOFDM		0x2000
/* bits 14-15 are reserved */

/* Management information elements */

enum {
	IEEE80211_ELEMID_SSID		= 0,
	IEEE80211_ELEMID_RATES		= 1,
	IEEE80211_ELEMID_FHPARMS	= 2,
	IEEE80211_ELEMID_DSPARMS	= 3,
	IEEE80211_ELEMID_CFPARMS	= 4,
	IEEE80211_ELEMID_TIM		= 5,
	IEEE80211_ELEMID_IBSSPARMS	= 6,
	IEEE80211_ELEMID_COUNTRY	= 7,
	IEEE80211_ELEMID_CHALLENGE	= 16,
	/* 17-31 reserved for challenge text extension */
	IEEE80211_ELEMID_ERP		= 42,
	IEEE80211_ELEMID_RSN		= 48,
	IEEE80211_ELEMID_XRATES		= 50,
	IEEE80211_ELEMID_TPC		= 150,
	IEEE80211_ELEMID_CCKM		= 156,
	IEEE80211_ELEMID_VENDOR		= 221,	/* vendor private */
};

/* Auth */

#define IEEE80211_AUTH_ALG_OPEN		0x0000
#define IEEE80211_AUTH_ALG_SHARED	0x0001

enum {
	IEEE80211_AUTH_OPEN_REQUEST		= 1,
	IEEE80211_AUTH_OPEN_RESPONSE		= 2,
};

enum {
	IEEE80211_AUTH_SHARED_REQUEST		= 1,
	IEEE80211_AUTH_SHARED_CHALLENGE		= 2,
	IEEE80211_AUTH_SHARED_RESPONSE		= 3,
	IEEE80211_AUTH_SHARED_PASS		= 4,
};

#define IEEE80211_CHALLENGE_LEN		128

/* Reason codes */

enum {
	IEEE80211_REASON_UNSPECIFIED		= 1,
	IEEE80211_REASON_AUTH_EXPIRE		= 2,
	IEEE80211_REASON_AUTH_LEAVE		= 3,
	IEEE80211_REASON_ASSOC_EXPIRE		= 4,
	IEEE80211_REASON_ASSOC_TOOMANY		= 5,
	IEEE80211_REASON_NOT_AUTHED		= 6,
	IEEE80211_REASON_NOT_ASSOCED		= 7,
	IEEE80211_REASON_ASSOC_LEAVE		= 8,
	IEEE80211_REASON_ASSOC_NOT_AUTHED	= 9,

	IEEE80211_REASON_RSN_REQUIRED		= 11,
	IEEE80211_REASON_RSN_INCONSISTENT	= 12,
	IEEE80211_REASON_IE_INVALID		= 13,
	IEEE80211_REASON_MIC_FAILURE		= 14,

	IEEE80211_STATUS_SUCCESS		= 0,
	IEEE80211_STATUS_UNSPECIFIED		= 1,
	IEEE80211_STATUS_CAPINFO		= 10,
	IEEE80211_STATUS_NOT_ASSOCED		= 11,
	IEEE80211_STATUS_OTHER			= 12,
	IEEE80211_STATUS_ALG			= 13,
	IEEE80211_STATUS_SEQUENCE		= 14,
	IEEE80211_STATUS_CHALLENGE		= 15,
	IEEE80211_STATUS_TIMEOUT		= 16,
	IEEE80211_STATUS_TOOMANY		= 17,
	IEEE80211_STATUS_BASIC_RATE		= 18,
	IEEE80211_STATUS_SP_REQUIRED		= 19,
	IEEE80211_STATUS_PBCC_REQUIRED		= 20,
	IEEE80211_STATUS_CA_REQUIRED		= 21,
	IEEE80211_STATUS_TOO_MANY_STATIONS	= 22,
	IEEE80211_STATUS_RATES			= 23,
	IEEE80211_STATUS_SHORTSLOT_REQUIRED	= 25,
	IEEE80211_STATUS_DSSSOFDM_REQUIRED	= 26,
};

#define IEEE80211_WEP64_KEYLEN		5
#define IEEE80211_WEP128_KEYLEN		13
#define IEEE80211_WEP_IVLEN		3

#define IEEE80211_MAX_KEYLEN		64

#define IEEE80211_MTU_MAX		2290
#define IEEE80211_MTU_MIN		32

#define IEEE80211_AUTH_TIMEOUT		10
#define IEEE80211_ASSOC_TIMEOUT		10
#define IEEE80211_RUN_TIMEOUT		600
#define IEEE80211_EAPOL_TIMEOUT		600
#define IEEE80211_STA_ASSOC_TIMEOUT	200
#define IEEE80211_STA_MAX_IDLE		3000

#define IEEE80211_SIFS			10

struct ieee80211_tkip_iv {
	union {
		struct {
			unsigned char rc0;
			unsigned char rc1;
			unsigned char rc2;
			union {
				struct {
					unsigned char reserved:5;
					unsigned char ext_iv:1;
					unsigned char key_id:2;
				} field;
				unsigned char byte;
			} control;
		} field;
		unsigned int word;
	} iv16;
	unsigned int iv32;
};

#pragma pack()

#endif /* _IEEE80211_H_ */

