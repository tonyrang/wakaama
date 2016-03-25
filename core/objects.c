/*******************************************************************************
 *
 * Copyright (c) 2013, 2014 Intel Corporation and others.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * The Eclipse Distribution License is available at
 *    http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    David Navarro, Intel Corporation - initial API and implementation
 *    Fabien Fleutot - Please refer to git log
 *    Toby Jaffey - Please refer to git log
 *    Benjamin Cabé - Please refer to git log
 *    Bosch Software Innovations GmbH - Please refer to git log
 *    Pascal Rieux - Please refer to git log
 *    
 *******************************************************************************/

/*
 Copyright (c) 2013, 2014 Intel Corporation

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

     * Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
     * Neither the name of Intel Corporation nor the names of its contributors
       may be used to endorse or promote products derived from this software
       without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 THE POSSIBILITY OF SUCH DAMAGE.

 David Navarro <david.navarro@intel.com>

*/
#include "internals.h"

#ifdef LWM2M_CLIENT_MODE


#include <stdlib.h>
#include <string.h>
#include <stdio.h>


static lwm2m_object_t * prv_findObject(lwm2m_context_t * contextP,
                                       uint16_t Id)
{
    int i;

    for (i = 0 ; i < contextP->numObject ; i++)
    {
        if (contextP->objectList[i]->objID == Id)
        {
            return contextP->objectList[i];
        }
    }

    return NULL;
}

uint8_t object_checkReadable(lwm2m_context_t * contextP,
                             lwm2m_uri_t * uriP)
{
    coap_status_t result;
    lwm2m_object_t * targetP;
    lwm2m_data_t * dataP = NULL;
    int size;

    targetP = prv_findObject(contextP, uriP->objectId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;
    if (NULL == targetP->readFunc) return COAP_405_METHOD_NOT_ALLOWED;

    if (!LWM2M_URI_IS_SET_INSTANCE(uriP)) return COAP_205_CONTENT;

    if (NULL == lwm2m_list_find(targetP->instanceList, uriP->instanceId)) return COAP_404_NOT_FOUND;

    if (!LWM2M_URI_IS_SET_RESOURCE(uriP)) return COAP_205_CONTENT;

    size = 1;
    dataP = lwm2m_data_new(1);
    if (dataP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;

    dataP->id = uriP->resourceId;

    result = targetP->readFunc(uriP->instanceId, &size, &dataP, targetP);
    lwm2m_data_free(1, dataP);

    return result;
}

uint8_t object_checkNumeric(lwm2m_context_t * contextP,
                            lwm2m_uri_t * uriP)
{
    coap_status_t result;
    lwm2m_object_t * targetP;
    lwm2m_data_t * dataP = NULL;
    int size;

    if (!LWM2M_URI_IS_SET_RESOURCE(uriP)) return COAP_405_METHOD_NOT_ALLOWED;

    targetP = prv_findObject(contextP, uriP->objectId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;
    if (NULL == targetP->readFunc) return COAP_405_METHOD_NOT_ALLOWED;

    size = 1;
    dataP = lwm2m_data_new(1);
    if (dataP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;

    dataP->id = uriP->resourceId;

    result = targetP->readFunc(uriP->instanceId, &size, &dataP, targetP);
    if (result == COAP_205_CONTENT)
    {
        switch (dataP->type)
        {
        case LWM2M_TYPE_INTEGER:
        case LWM2M_TYPE_FLOAT:
            break;
        default:
            result = COAP_405_METHOD_NOT_ALLOWED;
        }
    }

    lwm2m_data_free(1, dataP);

    return result;
}

coap_status_t object_readData(lwm2m_context_t * contextP,
                               lwm2m_uri_t * uriP,
                               int * sizeP,
                               lwm2m_data_t ** dataP)
{
    coap_status_t result;
    lwm2m_object_t * targetP;

    targetP = prv_findObject(contextP, uriP->objectId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;
    if (NULL == targetP->readFunc) return COAP_405_METHOD_NOT_ALLOWED;
    if (targetP->instanceList == NULL) return COAP_404_NOT_FOUND;

    if (LWM2M_URI_IS_SET_INSTANCE(uriP))
    {
        if (NULL == lwm2m_list_find(targetP->instanceList, uriP->instanceId)) return COAP_404_NOT_FOUND;

        // single instance read
        if (LWM2M_URI_IS_SET_RESOURCE(uriP))
        {
            *sizeP = 1;
            *dataP = lwm2m_data_new(*sizeP);
            if (*dataP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;

            (*dataP)->id = uriP->resourceId;
        }

        result = targetP->readFunc(uriP->instanceId, sizeP, dataP, targetP);
    }
    else
    {
        // multiple object instances read
        lwm2m_list_t * instanceP;
        int i;

        *sizeP = 0;
        for (instanceP = targetP->instanceList; instanceP != NULL ; instanceP = instanceP->next)
        {
            (*sizeP)++;
        }

        *dataP = lwm2m_data_new(*sizeP);
        if (*dataP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;

        result = COAP_205_CONTENT;
        instanceP = targetP->instanceList;
        i = 0;
        while (instanceP != NULL && result == COAP_205_CONTENT)
        {
            result = targetP->readFunc(instanceP->id, (int*)&((*dataP)[i].value.asChildren.num), &((*dataP)[i].value.asChildren.array), targetP);
            (*dataP)[i].type = LWM2M_TYPE_OBJECT_INSTANCE;
            (*dataP)[i].id = instanceP->id;
            i++;
            instanceP = instanceP->next;
        }
    }

    return result;
}

coap_status_t object_read(lwm2m_context_t * contextP,
                          lwm2m_uri_t * uriP,
                          lwm2m_media_type_t * formatP,
                          uint8_t ** bufferP,
                          size_t * lengthP)
{
    coap_status_t result;
    lwm2m_object_t * targetP;
    lwm2m_data_t * dataP = NULL;
    int size = 0;

    result = object_readData(contextP, uriP, &size, &dataP);

    if (result == COAP_205_CONTENT)
    {
        *lengthP = lwm2m_data_serialize(uriP, size, dataP, formatP, bufferP);
        if (*lengthP == 0) result = COAP_500_INTERNAL_SERVER_ERROR;
    }
    lwm2m_data_free(size, dataP);

    return result;
}

coap_status_t object_write(lwm2m_context_t * contextP,
                           lwm2m_uri_t * uriP,
                           lwm2m_media_type_t format,
                           uint8_t * buffer,
                           size_t length)
{
    coap_status_t result = NO_ERROR;
    lwm2m_object_t * targetP;
    lwm2m_data_t * dataP = NULL;
    int size = 0;

    targetP = prv_findObject(contextP, uriP->objectId);
    if (NULL == targetP)
    {
        result = COAP_404_NOT_FOUND;
    }
    else if (NULL == targetP->writeFunc)
    {
        result = COAP_405_METHOD_NOT_ALLOWED;
    }
    else
    {
        size = lwm2m_data_parse(uriP, buffer, length, format, &dataP);
        if (size == 0)
        {
            result = COAP_406_NOT_ACCEPTABLE;
        }
    }
    if (result == NO_ERROR)
    {
        result = targetP->writeFunc(uriP->instanceId, size, dataP, targetP);
        lwm2m_data_free(size, dataP);
    }
    return result;
}

coap_status_t object_execute(lwm2m_context_t * contextP,
                             lwm2m_uri_t * uriP,
                             uint8_t * buffer,
                             size_t length)
{
    lwm2m_object_t * targetP;

    targetP = prv_findObject(contextP, uriP->objectId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;
    if (NULL == targetP->executeFunc) return COAP_405_METHOD_NOT_ALLOWED;

    return targetP->executeFunc(uriP->instanceId, uriP->resourceId, buffer, length, targetP);
}

coap_status_t object_create(lwm2m_context_t * contextP,
                            lwm2m_uri_t * uriP,
                            lwm2m_media_type_t format,
                            uint8_t * buffer,
                            size_t length)
{
    lwm2m_object_t * targetP;
    lwm2m_data_t * dataP = NULL;
    int size = 0;
    uint8_t result;

    if (length == 0 || buffer == 0)
    {
        return COAP_400_BAD_REQUEST;
    }

    targetP = prv_findObject(contextP, uriP->objectId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;
    if (NULL == targetP->createFunc) return COAP_405_METHOD_NOT_ALLOWED;

    if (LWM2M_URI_IS_SET_INSTANCE(uriP))
    {
        if (NULL != lwm2m_list_find(targetP->instanceList, uriP->instanceId))
        {
            // Instance already exists
            return COAP_406_NOT_ACCEPTABLE;
        }
    }
    else
    {
        uriP->instanceId = lwm2m_list_newId(targetP->instanceList);
        uriP->flag |= LWM2M_URI_FLAG_INSTANCE_ID;
    }

    size = lwm2m_data_parse(uriP, buffer, length, format, &dataP);
    if (size == 0) return COAP_500_INTERNAL_SERVER_ERROR;
    result = targetP->createFunc(uriP->instanceId, size, dataP, targetP);
    lwm2m_data_free(size, dataP);

    return result;
}

coap_status_t object_delete(lwm2m_context_t * contextP,
                            lwm2m_uri_t * uriP)
{
    lwm2m_object_t * objectP;
    coap_status_t result;

    objectP = prv_findObject(contextP, uriP->objectId);
    if (NULL == objectP) return COAP_404_NOT_FOUND;
    if (NULL == objectP->deleteFunc) return COAP_405_METHOD_NOT_ALLOWED;

    LOG("    Call to object_delete\r\n");

    if (LWM2M_URI_IS_SET_RESOURCE(uriP))
    {
        result = objectP->deleteFunc(uriP->instanceId, objectP);
    }
    else
    {
        lwm2m_list_t * instanceP;

        result = COAP_202_DELETED;
        instanceP = objectP->instanceList;
        while (NULL != instanceP
            && result == COAP_202_DELETED)
        {
            result = objectP->deleteFunc(instanceP->id, objectP);
            instanceP = objectP->instanceList;
        }
    }

    return result;
}

coap_status_t object_discover(lwm2m_context_t * contextP,
                              lwm2m_uri_t * uriP,
                              uint8_t ** bufferP,
                              size_t * lengthP)
{
    coap_status_t result;
    lwm2m_object_t * targetP;
    lwm2m_data_t * dataP = NULL;
    int size = 0;

    targetP = prv_findObject(contextP, uriP->objectId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;
    if (NULL == targetP->discoverFunc) return COAP_501_NOT_IMPLEMENTED;
    if (targetP->instanceList == NULL) return COAP_404_NOT_FOUND;

    if (LWM2M_URI_IS_SET_INSTANCE(uriP))
    {
        if (NULL == lwm2m_list_find(targetP->instanceList, uriP->instanceId)) return COAP_404_NOT_FOUND;

        // single instance read
        if (LWM2M_URI_IS_SET_RESOURCE(uriP))
        {
            size = 1;
            dataP = lwm2m_data_new(size);
            if (dataP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;

            dataP->id = uriP->resourceId;
            uriP->flag &= ~LWM2M_URI_FLAG_RESOURCE_ID;
        }

        result = targetP->discoverFunc(uriP->instanceId, &size, &dataP, targetP);
    }
    else
    {
        // multiple object instances read
        lwm2m_list_t * instanceP;
        int i;

        size = 0;
        for (instanceP = targetP->instanceList; instanceP != NULL ; instanceP = instanceP->next)
        {
            size++;
        }

        dataP = lwm2m_data_new(size);
        if (dataP == NULL) return COAP_500_INTERNAL_SERVER_ERROR;

        result = COAP_205_CONTENT;
        instanceP = targetP->instanceList;
        i = 0;
        while (instanceP != NULL && result == COAP_205_CONTENT)
        {
            result = targetP->discoverFunc(instanceP->id, (int*)&(dataP[i].value.asChildren.num), &(dataP[i].value.asChildren.array), targetP);
            dataP[i].type = LWM2M_TYPE_OBJECT_INSTANCE;
            dataP[i].id = instanceP->id;
            i++;
            instanceP = instanceP->next;
        }
    }

    if (result == COAP_205_CONTENT)
    {
        int len;

        len = discover_serialize(contextP, uriP, size, dataP, bufferP);
        if (len <= 0) result = COAP_500_INTERNAL_SERVER_ERROR;
        else *lengthP = len;
    }
    lwm2m_data_free(size, dataP);

    return result;
}

bool object_isInstanceNew(lwm2m_context_t * contextP,
                          uint16_t objectId,
                          uint16_t instanceId)
{
    lwm2m_object_t * targetP;

    targetP = prv_findObject(contextP, objectId);
    if (targetP != NULL)
    {
        if (NULL != lwm2m_list_find(targetP->instanceList, instanceId))
        {
            return false;
        }
    }

    return true;
}

static int prv_getObjectTemplate(uint8_t * buffer,
                                 size_t length,
                                 uint16_t id)
{
    int index;
    int result;

    if (length < REG_OBJECT_MIN_LEN) return -1;

    buffer[0] = '<';
    buffer[1] = '/';
    index = 2;

    result = utils_intCopy(buffer + index, length - index, id);
    if (result < 0) return -1;
    index += result;

    if (length - index < REG_OBJECT_MIN_LEN - 3) return -1;
    buffer[index] = '/';
    index++;

    return index;
}

int object_getRegisterPayload(lwm2m_context_t * contextP,
                           uint8_t * buffer,
                           size_t bufferLen)
{
    int index;
    int result;
    int i;

    // index can not be greater than bufferLen
    index = 0;

    result = utils_stringCopy(buffer, bufferLen, REG_START);
    if (result < 0) return 0;
    index += result;

    if ((contextP->altPath != NULL)
     && (contextP->altPath[0] != 0))
    {
        result = utils_stringCopy(buffer + index, bufferLen - index, contextP->altPath);
    }
    else
    {
        result = utils_stringCopy(buffer + index, bufferLen - index, REG_DEFAULT_PATH);
    }
    if (result < 0) return 0;
    index += result;

    result = utils_stringCopy(buffer + index, bufferLen - index, REG_LWM2M_RESOURCE_TYPE);
    if (result < 0) return 0;
    index += result;

    for (i = 0 ; i < contextP->numObject ; i++)
    {
        int start;
        int length;

        if (contextP->objectList[i]->objID == LWM2M_SECURITY_OBJECT_ID) continue;

        start = index;
        length = prv_getObjectTemplate(buffer + index, bufferLen - index, contextP->objectList[i]->objID);
        if (length < 0) return 0;
        index += length;

        if (contextP->objectList[i]->instanceList == NULL)
        {
            index--;
            result = utils_stringCopy(buffer + index, bufferLen - index, REG_PATH_END);
            if (result < 0) return 0;
            index += result;
        }
        else
        {
            lwm2m_list_t * targetP;
            for (targetP = contextP->objectList[i]->instanceList ; targetP != NULL ; targetP = targetP->next)
            {
                if (bufferLen - index <= length) return 0;

                if (index != start + length)
                {
                    memcpy(buffer + index, buffer + start, length);
                    index += length;
                }

                result = utils_intCopy(buffer + index, bufferLen - index, targetP->id);
                if (result < 0) return 0;
                index += result;

                result = utils_stringCopy(buffer + index, bufferLen - index, REG_PATH_END);
                if (result < 0) return 0;
                index += result;
            }
        }
    }

    if (index > 0)
    {
        index = index - 1;  // remove trailing ','
    }

    buffer[index] = 0;

    return index;
}

static lwm2m_list_t * prv_findServerInstance(lwm2m_object_t * objectP,
                                             uint16_t shortID)
{
    lwm2m_list_t * instanceP;

    instanceP = objectP->instanceList;
    while (NULL != instanceP)
    {
        int64_t value;
        lwm2m_data_t * dataP;
        int size;

        size = 1;
        dataP = lwm2m_data_new(size);
        if (dataP == NULL) return NULL;
        dataP->id = LWM2M_SERVER_SHORT_ID_ID;

        if (objectP->readFunc(instanceP->id, &size, &dataP, objectP) != COAP_205_CONTENT)
        {
            lwm2m_data_free(size, dataP);
            return NULL;
        }

        if (1 == lwm2m_data_decode_int(dataP, &value))
        {
            if (value == shortID)
            {
                lwm2m_data_free(size, dataP);
                break;
            }
        }
        lwm2m_data_free(size, dataP);
        instanceP = instanceP->next;
    }

    return instanceP;
}

static int prv_getMandatoryInfo(lwm2m_object_t * objectP,
                                uint16_t instanceID,
                                lwm2m_server_t * targetP)
{
    lwm2m_data_t * dataP;
    int size;
    int64_t value;

    size = 2;
    dataP = lwm2m_data_new(size);
    if (dataP == NULL) return -1;
    dataP[0].id = LWM2M_SERVER_LIFETIME_ID;
    dataP[1].id = LWM2M_SERVER_BINDING_ID;

    if (objectP->readFunc(instanceID, &size, &dataP, objectP) != COAP_205_CONTENT)
    {
        lwm2m_data_free(size, dataP);
        return -1;
    }

    if (0 == lwm2m_data_decode_int(dataP, &value)
     || value < 0 || value >0xFFFFFFFF)             // This is an implementation limit
    {
        lwm2m_data_free(size, dataP);
        return -1;
    }
    targetP->lifetime = value;

    targetP->binding = utils_stringToBinding(dataP[1].value.asBuffer.buffer, dataP[1].value.asBuffer.length);

    lwm2m_data_free(size, dataP);

    if (targetP->binding == BINDING_UNKNOWN)
    {
        return -1;
    }

    return 0;
}

int object_getServers(lwm2m_context_t * contextP)
{
    lwm2m_object_t * securityObjP = NULL;
    lwm2m_object_t * serverObjP = NULL;
    lwm2m_list_t * securityInstP;   // instanceID of the server in the LWM2M Security Object
    int i;

    for (i = 0 ; i < contextP->numObject ; i++)
    {
        if (contextP->objectList[i]->objID == LWM2M_SECURITY_OBJECT_ID)
        {
            securityObjP = contextP->objectList[i];
        }
        else if (contextP->objectList[i]->objID == LWM2M_SERVER_OBJECT_ID)
        {
            serverObjP = contextP->objectList[i];
        }
    }

    if (NULL == securityObjP) return -1;

    securityInstP = securityObjP->instanceList;
    while (securityInstP != NULL)
    {
        if (LWM2M_LIST_FIND(contextP->bootstrapServerList, securityInstP->id) == NULL
         && LWM2M_LIST_FIND(contextP->serverList, securityInstP->id) == NULL)
        {
            // This server is new. eg created by last bootstrap

            lwm2m_data_t * dataP;
            int size;
            lwm2m_server_t * targetP;
            bool isBootstrap;
            int64_t value = 0;

            size = 3;
            dataP = lwm2m_data_new(size);
            if (dataP == NULL) return -1;
            dataP[0].id = LWM2M_SECURITY_BOOTSTRAP_ID;
            dataP[1].id = LWM2M_SECURITY_SHORT_SERVER_ID;
            dataP[2].id = LWM2M_SECURITY_HOLD_OFF_ID;

            if (securityObjP->readFunc(securityInstP->id, &size, &dataP, securityObjP) != COAP_205_CONTENT)
            {
                lwm2m_data_free(size, dataP);
                return -1;
            }

            targetP = (lwm2m_server_t *)lwm2m_malloc(sizeof(lwm2m_server_t));
            if (targetP == NULL) {
                lwm2m_data_free(size, dataP);
                return -1;
            }
            memset(targetP, 0, sizeof(lwm2m_server_t));
            targetP->secObjInstID = securityInstP->id;

            if (0 == lwm2m_data_decode_bool(dataP + 0, &isBootstrap))
            {
                lwm2m_free(targetP);
                lwm2m_data_free(size, dataP);
                return -1;
            }

            if (0 == lwm2m_data_decode_int(dataP + 1, &value)
             || value < (isBootstrap ? 0 : 1) || value > 0xFFFF)                // 0 is forbidden as a Short Server ID
            {
                lwm2m_free(targetP);
                lwm2m_data_free(size, dataP);
                return -1;
            }
            targetP->shortID = value;

            if (isBootstrap == true)
            {
                if (0 == lwm2m_data_decode_int(dataP + 2, &value)
                 || value < 0 || value > 0xFFFFFFFF)             // This is an implementation limit
                {
                    lwm2m_free(targetP);
                    lwm2m_data_free(size, dataP);
                    return -1;
                }
                // lifetime of a bootstrap server is set to ClientHoldOffTime
                targetP->lifetime = value;

                contextP->bootstrapServerList = (lwm2m_server_t*)LWM2M_LIST_ADD(contextP->bootstrapServerList, targetP);
            }
            else
            {
                lwm2m_list_t * serverInstP;     // instanceID of the server in the LWM2M Server Object

                serverInstP = prv_findServerInstance(serverObjP, targetP->shortID);
                if (serverInstP == NULL)
                {
                    lwm2m_free(targetP);
                    lwm2m_data_free(size, dataP);
                    return -1;
                }
                if (0 != prv_getMandatoryInfo(serverObjP, serverInstP->id, targetP))
                {
                    lwm2m_free(targetP);
                    lwm2m_data_free(size, dataP);
                    return -1;
                }
                targetP->status = STATE_DEREGISTERED;
                contextP->serverList = (lwm2m_server_t*)LWM2M_LIST_ADD(contextP->serverList, targetP);
            }
            lwm2m_data_free(size, dataP);
        }
        securityInstP = securityInstP->next;
    }

    return 0;
}

coap_status_t object_createInstance(lwm2m_context_t * contextP,
                                    lwm2m_uri_t * uriP,
                                    lwm2m_data_t * dataP)
{
    lwm2m_object_t * targetP;
    coap_status_t result;

    targetP = prv_findObject(contextP, uriP->objectId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;

    if (NULL == targetP->createFunc) 
    {
        return COAP_405_METHOD_NOT_ALLOWED;
    }

    result = targetP->createFunc(lwm2m_list_newId(targetP->instanceList), dataP->value.asChildren.num, dataP->value.asChildren.array, targetP);
}

coap_status_t object_writeInstance(lwm2m_context_t * contextP,
                            lwm2m_uri_t * uriP,
                            lwm2m_data_t * dataP)
{
    lwm2m_object_t * targetP;
    coap_status_t result;

    targetP = prv_findObject(contextP, uriP->objectId);
    if (NULL == targetP) return COAP_404_NOT_FOUND;

    if (NULL == targetP->writeFunc) 
    {
        return COAP_405_METHOD_NOT_ALLOWED;
    }

    result = targetP->writeFunc(dataP->id, dataP->value.asChildren.num, dataP->value.asChildren.array, targetP);
}

#endif
