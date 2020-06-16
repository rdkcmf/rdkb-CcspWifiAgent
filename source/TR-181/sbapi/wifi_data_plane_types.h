#ifndef WIFI_DATA_PLANE_TYPE_H
#define WIFI_DATA_PLANE_TYPE_H

typedef enum {
    wifi_data_plane_packet_type_8021x,
    wifi_data_plane_packet_type_auth,
    wifi_data_plane_packet_type_assoc_req,
    wifi_data_plane_packet_type_assoc_rsp,
} wifi_data_plane_packet_type_t;

typedef enum {
    wifi_data_plane_event_type_dpp,
    wifi_data_plane_event_type_anqp,
} wifi_data_plane_event_type_t;

typedef enum {
    wifi_data_plane_queue_data_type_packet,
    wifi_data_plane_queue_data_type_event,
} wifi_data_plane_queue_data_type_t;

#endif
