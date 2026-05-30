#ifndef RESOURCE_H
#define RESOURCE_H

#define IDI_RCLONE                 201
#define IDI_RCLONE_SMALL           202

#define IDD_RCLONE_CONFIG          101
#define IDD_JOB_PROGRESS           102
#define IDD_FILE_PROPERTIES        103
#define IDD_SHARE_DIALOG           104
#define IDD_VERSIONS_DIALOG        105
#define IDD_SYNC_DIALOG            106
#define IDD_CHECK_DIALOG            107
#define IDD_JOB_DIALOG               108
#define IDD_TRASH_DIALOG             109

#define IDC_PROGRESS_BAR           3001
#define IDC_PROGRESS_TEXT          3002
#define IDC_PROGRESS_PERCENT       3003

#define IDC_PROP_NAME              4001
#define IDC_PROP_TYPE              4002
#define IDC_PROP_SIZE              4003
#define IDC_PROP_MTIME             4004
#define IDC_PROP_MIME              4005
#define IDC_PROP_FID               4006
#define IDC_PROP_RPATH             4007
#define IDC_PROP_HASH              4008
#define IDC_PROP_HASHTYPE          4009
#define IDC_PROP_QUOTA_TOTAL       4010
#define IDC_PROP_QUOTA_USED        4011
#define IDC_PROP_QUOTA_FREE        4012
#define IDC_PROP_COPY              4013

// Share Dialog controls
#define IDC_SHARE_URL              5001
#define IDC_SHARE_EXPIRE           5002
#define IDC_SHARE_OPEN_BROWSER     5003
#define IDC_SHARE_COPY_LINK        5004
#define IDC_SHARE_STATUS           5005
#define IDC_SHARE_PROGRESS         5006
#define IDC_SHARE_GENERATE         5007

// Versions Dialog controls
#define IDC_VERSION_LIST           5011
#define IDC_VERSION_RESTORE        5012
#define IDC_VERSION_PREVIEW        5013
#define IDC_VERSION_DELETE         5014
#define IDC_VERSION_STATUS         5015

// Sync Dialog controls
#define IDC_SYNC_SRC_PATH          5021
#define IDC_SYNC_DST_PATH          5022
#define IDC_SYNC_MODE              5023
#define IDC_SYNC_START             5024
#define IDC_SYNC_PROGRESS          5025
#define IDC_SYNC_STATUS            5026

// Check Dialog controls
#define IDC_CHECK_SRC_PATH         5031
#define IDC_CHECK_DST_PATH         5032
#define IDC_CHECK_ONEWAY           5033
#define IDC_CHECK_START            5034
#define IDC_CHECK_PROGRESS         5035
#define IDC_CHECK_STATUS           5036
#define IDC_CHECK_REPORT           5037

// Job Dialog controls
#define IDC_JOB_LIST                5041
#define IDC_JOB_REFRESH             5042
#define IDC_JOB_STOP                5043
#define IDC_JOB_STOP_ALL            5044
#define IDC_JOB_STATUS              5045

// Trash Dialog controls
#define IDC_TRASH_LIST              5051
#define IDC_TRASH_RESTORE           5052
#define IDC_TRASH_DELETE            5053
#define IDC_TRASH_EMPTY             5054
#define IDC_TRASH_STATUS            5055

#define IDC_NAV_LIST               1001
#define IDC_RCLONE_PATH            1002
#define IDC_BROWSE_RCLONE          1003
#define IDC_RC_PORT                1004
#define IDC_RC_USER                1005
#define IDC_RC_PASS                1006
#define IDC_CACHE_ENABLED          1007
#define IDC_CACHE_TTL              1008
#define IDC_MAX_CONNECTIONS        1009
#define IDC_BANDWIDTH_LIMIT        1010
#define IDC_AUTO_SYNC              1011
#define IDC_SYNC_INTERVAL          1012
#define IDC_VERBOSE_LOGGING        1013
#define IDC_TEST_CONN              1014
#define IDC_OPEN_CONFIG            1015
#define IDC_RESET_DEFAULTS         1016
#define IDC_CONFIG_PATH            1017
#define IDC_BROWSE_CONFIG          1018
#define IDC_CACHE_DIR              1019
#define IDC_BROWSE_CACHE_DIR       1020
#define IDC_TRANSFERS              1021
#define IDC_MAX_IDLE               1022
#define IDC_CHECKERS               1023
#define IDC_CONFIRM_DELETE         1024
#define IDC_SHOW_HIDDEN            1025
#define IDC_CASE_SENSITIVE         1026
#define IDC_CACHE_SIZE             1027
#define IDC_BUFFER_SIZE            1028
#define IDC_READ_AHEAD             1029
#define IDC_LOW_LEVEL_RETRIES      1030
#define IDC_STATS_INTERVAL         1031
#define IDC_CACHE_WRITE_BACK       1032
#define IDC_DIR_CACHE_TIME         1033
#define IDC_VFS_CACHE_MODE         1034
#define IDC_CONN_TIMEOUT           1035
#define IDC_CACHE_MAX_AGE          1036
#define IDC_READ_CHUNK_SIZE        1037
#define IDC_WRITE_BACK_DELAY       1038
#define IDC_NO_MODTIME             1039
#define IDC_NO_CHECKSUM            1040
#define IDC_AUTO_START_DAEMON      1041
#define IDC_POLL_INTERVAL          1042
#define IDC_FAST_LIST              1043
#define IDC_USE_MMAP               1044
#define IDC_LOG_ENABLED            1045
#define IDC_LOG_PATH               1046
#define IDC_BROWSE_LOG             1047
#define IDC_LOG_LEVEL              1048
#define IDC_LOG_MAX_SIZE           1049
#define IDC_LOG_MAX_FILES          1050
#define IDC_OPEN_LOG               1051
#define IDC_CLEAR_LOG              1052
#define IDC_USE_RCLONE_COPY        1053

#define IDC_LBL_RCLONE_PATH       2101
#define IDC_LBL_RC_PORT           2102
#define IDC_LBL_RC_USER           2103
#define IDC_LBL_RC_PASS           2104
#define IDC_LBL_CACHE_TTL         2105
#define IDC_LBL_MAX_CONN          2106
#define IDC_LBL_BANDWIDTH         2107
#define IDC_LBL_SYNC_INTERVAL     2108
#define IDC_LBL_CONFIG_PATH       2109
#define IDC_LBL_CACHE_DIR         2110
#define IDC_LBL_TRANSFERS         2111
#define IDC_LBL_MAX_IDLE          2112
#define IDC_LBL_CHECKERS          2113
#define IDC_LBL_CACHE_SIZE        2114
#define IDC_LBL_BUFFER_SIZE       2115
#define IDC_LBL_READ_AHEAD        2116
#define IDC_LBL_LOW_LEVEL_RETRIES 2117
#define IDC_LBL_STATS_INTERVAL    2118
#define IDC_LBL_DIR_CACHE_TIME    2119
#define IDC_LBL_VFS_CACHE_MODE    2120
#define IDC_LBL_CONN_TIMEOUT      2121
#define IDC_LBL_CACHE_MAX_AGE     2122
#define IDC_LBL_READ_CHUNK_SIZE   2123
#define IDC_LBL_WRITE_BACK_DELAY  2124
#define IDC_LBL_POLL_INTERVAL     2125
#define IDC_LBL_LOG_PATH          2126
#define IDC_LBL_LOG_LEVEL         2127
#define IDC_LBL_LOG_MAX_SIZE      2128
#define IDC_LBL_LOG_MAX_FILES     2129

#endif
