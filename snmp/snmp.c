#include "snmp.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "asn1.h"


typedef struct VarbindList
{
    char *oid;
    int value_type;
    Value value;
    
    struct VarbindList *next;
} VarbindList;

struct SNMPMessage
{
    int version;
    char *community;
    int pdu_type;
    int request_id;
    int error;
    int error_index;
    VarbindList *varbind_list;
};

SNMPMessage *snmp_create_message()
{
    SNMPMessage *message = malloc(sizeof(SNMPMessage));
    message->version = 0;
    message->community = NULL;
    message->pdu_type = 0;
    message->request_id = 0;
    message->error = 0;
    message->error_index = 0;
    message->varbind_list = NULL;
    return message;
}

static void destroy_varbind_list(VarbindList *list)
{
    if (list == NULL)
        return;
    
    free(list->oid);
    if (list->value_type == SNMP_STRING_TYPE)
        free(list->value.str_value);
    destroy_varbind_list(list->next);
    free(list);
}

void snmp_destroy_message(SNMPMessage *message)
{
    free(message->community);
    destroy_varbind_list(message->varbind_list);
}

void snmp_set_version(SNMPMessage *message, int version)
{
    message->version = version;
}

void snmp_set_community(SNMPMessage *message, char *community)
{
    free(message->community);
    message->community = strdup(community);
}

void snmp_set_pdu_type(SNMPMessage *message, int type)
{
    message->pdu_type = type;
}

void snmp_set_request_id(SNMPMessage *message, int request_id)
{
    message->request_id = request_id;
}

void snmp_set_error(SNMPMessage *message, int error)
{
    message->error = error;
}

void snmp_set_error_index(SNMPMessage *message, int error_index)
{
    message->error_index = error_index;
}

void snmp_add_varbind(SNMPMessage *message, VarbindList *vb)
{
    if (message->varbind_list == NULL)
        message->varbind_list = vb;
    else
    {
        VarbindList *parent = message->varbind_list;
        
        while (parent->next != NULL)
            parent = parent->next;
        
        parent->next = vb;
    }
}

void snmp_add_varbind_null(SNMPMessage *message, char *oid)
{
    VarbindList *vb = malloc(sizeof (VarbindList));
    vb->oid = strdup(oid);
    vb->value_type = SNMP_NULL_TYPE;
    vb->value.str_value = NULL;
    vb->next = NULL;
    
    snmp_add_varbind(message, vb);
}

void snmp_add_varbind_integer(SNMPMessage *message, char *oid, int value)
{
    VarbindList *vb = malloc(sizeof (VarbindList));
    vb->oid = strdup(oid);
    vb->value_type = SNMP_INTEGER_TYPE;
    vb->value.int_value = value;
    vb->next = NULL;
    
    snmp_add_varbind(message, vb);
}

void snmp_add_varbind_string(SNMPMessage *message, char *oid, char *value)
{
    VarbindList *vb = malloc(sizeof (VarbindList));
    vb->oid = strdup(oid);
    vb->value_type = SNMP_STRING_TYPE;
    vb->value.str_value = strdup(value);
    vb->next = NULL;
    
    snmp_add_varbind(message, vb);
}

static void get_msg_lens(SNMPMessage *message, int *msg_len, int *pdu_len, int *vbl_len)
{
    *vbl_len = 0;
    VarbindList *vb = message->varbind_list;
    while (vb != NULL)
    {
        int oid_obj_len = object_length(oid_length(vb->oid));
        int value_obj_len = object_length(value_length(vb->value_type, vb->value));
        *vbl_len += object_length(oid_obj_len + value_obj_len);
        
        vb = vb->next;
    }
    
    *pdu_len = object_length(integer_length(message->request_id));
    *pdu_len += object_length(integer_length(message->error));
    *pdu_len += object_length(integer_length(message->error_index));
    *pdu_len += sequence_header_length(*vbl_len) + *vbl_len;
    
    *msg_len = object_length(integer_length(message->version));
    *msg_len += object_length(string_length(message->community));
    
    *msg_len += header_length(message->pdu_type, *pdu_len) + *pdu_len;
}

int snmp_message_length(SNMPMessage *message)
{
    int msg_len, pdu_len, vbl_len;
    
    get_msg_lens(message, &msg_len, &pdu_len, &vbl_len);
    
    return sequence_header_length(msg_len) + msg_len;
}

void snmp_render_message(SNMPMessage *message, void *buffer)
{
    int msg_len, pdu_len, vbl_len;
    VarbindList *vb;
    void *p = buffer;
    
    get_msg_lens(message, &msg_len, &pdu_len, &vbl_len);
    
    p = render_sequence_header(msg_len, p);
    p = render_integer_object(message->version, p);
    p = render_string_object(message->community, p);
    
    p = render_header(message->pdu_type, pdu_len, p);
    p = render_integer_object(message->request_id, p);
    p = render_integer_object(message->error, p);
    p = render_integer_object(message->error_index, p);
    
    p = render_sequence_header(vbl_len, p);
    vb = message->varbind_list;
    while (vb != NULL)
    {
        int oid_obj_len = object_length(oid_length(vb->oid));
        int value_obj_len = object_length(value_length(vb->value_type, vb->value));
        p = render_sequence_header(oid_obj_len + value_obj_len, p);
        p = render_oid_object(vb->oid, p);
        p = render_value_object(vb->value_type, vb->value, p);
        
        vb = vb->next;
    }
}

SNMPMessage *snmp_parse_message(void *buffer, int len)
{
    SNMPMessage *message = snmp_create_message();
    ASN1Parser *parser = asn1_create_parser(buffer, len);
    
    asn1_parse_sequence(parser);
    asn1_parse_integer(parser, &message->version);
    asn1_parse_string(parser, &message->community);
    asn1_parse_structure(parser, &message->pdu_type);
    asn1_parse_integer(parser, &message->request_id);
    asn1_parse_integer(parser, &message->error);
    asn1_parse_integer(parser, &message->error_index);
    asn1_parse_sequence(parser);
    while (asn1_parse_sequence(parser))
    {
        char *oid;
        int type;
        Value value;
        asn1_parse_oid(parser, &oid);
        asn1_parse_value(parser, &type, &value);
        asn1_parse_pop(parser);
        
        switch (type)
        {
            case SNMP_NULL_TYPE:
                snmp_add_varbind_null(message, oid);
                break;
            case SNMP_INTEGER_TYPE:
                snmp_add_varbind_integer(message, oid, value.int_value);
                break;
            case SNMP_STRING_TYPE:
                snmp_add_varbind_string(message, oid, value.str_value);
                free(value.str_value);
                break;
        }
    }
    asn1_parse_pop(parser);
    asn1_parse_pop(parser);
    asn1_parse_pop(parser);
    asn1_destroy_parser(parser);
    
    return message;
}

void snmp_print_message(SNMPMessage *message)
{
    VarbindList *vb;
    
    printf("SNMP Message:\n");
    printf("    Version: %d\n", message->version);
    printf("    Community: %s\n", message->community);
    printf("    PDU Type: %d\n", message->pdu_type);
    printf("    Request ID: %d\n", message->request_id);
    printf("    Error: %d\n", message->error);
    printf("    Error Index: %d\n", message->error_index);
    
    vb = message->varbind_list;
    while (vb)
    {
        printf("        OID: %s\n", vb->oid);
        switch (vb->value_type)
        {
            case SNMP_NULL_TYPE:
                printf("            Null\n");
                break;
            case SNMP_INTEGER_TYPE:
                printf("            Integer: %d\n", vb->value.int_value);
                break;
            case SNMP_STRING_TYPE:
                printf("            String: %s\n", vb->value.str_value);
                break;
        }
        vb = vb->next;
    }
}

void test()
{
    SNMPMessage *message = snmp_create_message();
    SNMPMessage *message2;
    int len;
    unsigned char *buf;
    int i;
    
    snmp_set_version(message, 0);
    snmp_set_community(message, "private");
    snmp_set_pdu_type(message, SNMP_GET_REQUEST_TYPE);
    snmp_set_request_id(message, 1);
    snmp_set_error(message, 0);
    snmp_set_error_index(message, 0);
    snmp_add_varbind_null(message, "1.3.6.1.4.1.2680.1.2.7.3.2.0");
    
    snmp_print_message(message);
    
    len = snmp_message_length(message);
    buf = malloc(len);
    snmp_render_message(message, buf);
    
    snmp_destroy_message(message);
    
    printf("%02x", buf[0]);
    for (i = 1; i < len; i++)
        printf(" %02x", buf[i]);
    printf("\n");
    
    message2 = snmp_parse_message(buf, len);
    snmp_print_message(message2);
    snmp_destroy_message(message2);
    
    free(buf);
}

#if 0
int main()
{
    test();
    return 0;
}
#endif
