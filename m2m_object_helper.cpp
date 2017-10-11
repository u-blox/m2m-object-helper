/* mbed Microcontroller Library
 * Copyright (c) 2017 u-blox
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mbed.h"
#include "MbedCloudClient.h"
#include "m2m_object_helper.h"

#define printfLog(format, ...) debug_if(_debugOn, format, ## __VA_ARGS__)

/**********************************************************************
 * PUBLIC METHODS
 **********************************************************************/

// Destructor.
M2MObjectHelper::~M2MObjectHelper()
{
    M2MObjectInstance *objectInstance;

    if (_object != NULL) {
        objectInstance = _object->object_instance(_defObject->instance);
        if (objectInstance != NULL) {
            _object->remove_object_instance(objectInstance->instance_id());
        }
        if (_object->instance_count() == 0) {
            delete _object;
        }
    }
}

// Default implementation of updateObservableResources.
void M2MObjectHelper::updateObservableResources()
{
}

// Create this object.
bool M2MObjectHelper::makeObject()
{
    bool allResourcesCreated = true;
    M2MObjectInstance *objectInstance;
    M2MResource *resource;
    M2MResourceInstance *resourceInstance;
    const DefResource *defResource;

    if (_defObject != NULL) {
        printfLog("M2MObjectHelper: making object \"%s\", instance %d (-1 == single instance), with %d resource(s).\n",
                  _defObject->name, _defObject->instance, _defObject->numResources);

        // Create the object according to the definition
        if (_object == NULL) {
            _object = M2MInterfaceFactory::create_object(_defObject->name);
        }
        if (_object != NULL) {
            // Create the resources according to the definition
            objectInstance = _object->create_object_instance(_defObject->instance);
            if (objectInstance != NULL) {
                for (int x = 0; x < _defObject->numResources; x++) {
                    defResource = &(_defObject->resources[x]);
                    // If this is a multi-instance resource, create the base
                    // instance if it's not already there
                    if ((defResource->instance != -1) && (objectInstance->resource(defResource->name) == NULL)) {
                        printfLog("M2MObjectHelper: creating base instance of multi-instance resource \"%s\" in object \"%s\".\n",
                                  defResource->name, _object->name());
                        resource = objectInstance->create_dynamic_resource((const char *) defResource->name,
                                                                           (const char *) defResource->typeString,
                                                                           defResource->type,
                                                                           defResource->observable,
                                                                           true /* multi-instance */);
                        if (resource == NULL) {
                            allResourcesCreated = false;
                            printfLog("M2MObjectHelper: unable to create base instance of multi-instance resource \"%s\" in object \"%s\".\n",
                                      defResource->name, _defObject->name);
                        }
                    }

                    // Now create the resource
                    if (defResource->instance >= 0) {
                        printfLog("M2MObjectHelper: creating instance %d of multi-instance resource \"%s\" in object \"%s\".\n",
                                  defResource->instance, defResource->name, _object->name());
                        resourceInstance = objectInstance->create_dynamic_resource_instance((const char *) defResource->name,
                                                                                            (const char *) defResource->typeString,
                                                                                            defResource->type,
                                                                                            defResource->observable,
                                                                                            defResource->instance);
                        if (resourceInstance != NULL) {
                            resourceInstance->set_operation(defResource->operation);
                            if (_valueUpdatedCallback) {
                                resourceInstance->set_value_updated_function(_valueUpdatedCallback);
                            }
                        } else {
                            allResourcesCreated = false;
                            printfLog("M2MObjectHelper: unable to create instance %d of multi-instance resource \"%s\" in object \"%s\".\n",
                                      defResource->instance, defResource->name, _defObject->name);
                        }
                    } else {
                        printfLog("M2MObjectHelper: creating single-instance resource \"%s\" in object \"%s\".\n",
                                  defResource->name, _object->name());
                        resource = objectInstance->create_dynamic_resource((const char *) defResource->name,
                                                                           (const char *) defResource->typeString,
                                                                           defResource->type,
                                                                           defResource->observable);
                        if (resource != NULL) {
                            resource->set_operation(defResource->operation);
                            if (_valueUpdatedCallback) {
                                resource->set_value_updated_function(_valueUpdatedCallback);
                            }
                        } else {
                            allResourcesCreated = false;
                            printfLog("M2MObjectHelper: unable to create single-instance resource \"%s\" in object \"%s\".\n",
                                      defResource->name, _defObject->name);
                        }
                    }
                }
            } else {
                printfLog("M2MObjectHelper: unable to create instance of object \"%s\".\n", _defObject->name);
            }
        } else {
            printfLog("M2MObjectHelper: unable to create object \"%s\".\n", _defObject->name);
        }
    } else {
        printfLog("M2MObjectHelper: defObject is NULL.\n");
    }

    return (objectInstance != NULL) && allResourcesCreated;
}

// Set the execute function for a resource.
bool M2MObjectHelper::setExecuteCallback(execute_callback callback, const char *resourceNumber)
{
    bool success = false;
    M2MObjectInstance *objectInstance;
    M2MResource *resource;

    if (_object != NULL) {
        printfLog("M2MObjectHelper: setting execute callback for resource \"%s\" in object \"%s\".\n",
                  resourceNumber, _object->name());
        objectInstance = _object->object_instance(_defObject->instance);
        if (objectInstance != NULL) {
            resource = objectInstance->resource(resourceNumber);
            if (resource != NULL) {
                success = resource->set_execute_function(callback);
            } else {
                printfLog("M2MObjectHelper: unable to find resource \"%s\" in object \"%s\".\n",
                          resourceNumber, _object->name());
            }
        } else {
            printfLog("M2MObjectHelper: unable to get instance of object \"%s\".\n", _object->name());
        }
    } else {
        printfLog("M2MObjectHelper: object is NULL.\n");
    }

    return success;
}


// Set the value of a given resource in an object.
bool M2MObjectHelper::setResourceValue(int64_t value,
                                       const char *resourceNumber,
                                       int wantedInstance)
{
    bool success = false;
    bool foundIt = false;
    M2MResourceBase::ResourceType type;

    // Find the resource type and format from the object definition
    for (int x = 0; (x < _defObject->numResources) && !foundIt; x++) {
        if ((strcmp(resourceNumber, _defObject->resources[x].name) == 0) &&
            (wantedInstance == _defObject->resources[x].instance)) {
            type = _defObject->resources[x].type;
            foundIt = true;
        }
    }

    if (foundIt &&
        ((type == M2MResourceBase::INTEGER) ||
          type == M2MResourceBase::TIME)) {
        success = setResourceValue((void *) &value,
                                   type,
                                   resourceNumber,
                                   wantedInstance);
    }

    return success;
}

// Set the value of a given resource in an object.
bool M2MObjectHelper::setResourceValue(float value,
                                       const char *resourceNumber,
                                       int wantedInstance)
{
    bool success = false;
    bool foundIt = false;
    M2MResourceBase::ResourceType type;

    // Find the resource type and format from the object definition
    for (int x = 0; (x < _defObject->numResources) && !foundIt; x++) {
        if ((strcmp(resourceNumber, _defObject->resources[x].name) == 0) &&
            (wantedInstance == _defObject->resources[x].instance)) {
            type = _defObject->resources[x].type;
            foundIt = true;
        }
    }

    if (foundIt && (type == M2MResourceBase::FLOAT)) {
        success = setResourceValue((void *) &value,
                                   type,
                                   resourceNumber,
                                   wantedInstance);
    }

    return success;
}

// Set the value of a given resource in an object.
bool M2MObjectHelper::setResourceValue(bool value,
                                       const char *resourceNumber,
                                       int wantedInstance)
{
    bool success = false;
    bool foundIt = false;
    M2MResourceBase::ResourceType type;

    // Find the resource type and format from the object definition
    for (int x = 0; (x < _defObject->numResources) && !foundIt; x++) {
        if ((strcmp(resourceNumber, _defObject->resources[x].name) == 0) &&
            (wantedInstance == _defObject->resources[x].instance)) {
            type = _defObject->resources[x].type;
            foundIt = true;
        }
    }

    if (foundIt && (type == M2MResourceBase::BOOLEAN)) {
        success = setResourceValue((void *) &value,
                                   type,
                                   resourceNumber,
                                   wantedInstance);
    }

    return success;

}

// Set the value of a given resource in an object.
bool M2MObjectHelper::setResourceValue(const char *value,
                                       const char *resourceNumber,
                                       int wantedInstance)
{
    bool success = false;
    bool foundIt = false;
    M2MResourceBase::ResourceType type;
    const char * format;
    String *str = new String(value);

    // Find the resource type and format from the object definition
    for (int x = 0; (x < _defObject->numResources) && !foundIt; x++) {
        if ((strcmp(resourceNumber, _defObject->resources[x].name) == 0) &&
            (wantedInstance == _defObject->resources[x].instance)) {
            type = _defObject->resources[x].type;
            format = _defObject->resources[x].format;
            foundIt = true;
        }
    }

    if (foundIt && (type == M2MResourceBase::STRING)) {
        success = setResourceValue((void *) str,
                                   type,
                                   resourceNumber,
                                   wantedInstance,
                                   format);
    }

    return success;
}

// Set the value of a given resource in an object.
bool M2MObjectHelper::setResourceValue(String value,
                                       const char *resourceNumber,
                                       int wantedInstance)
{
    bool success = false;
    bool foundIt = false;
    M2MResourceBase::ResourceType type;
    const char * format;

    // Find the resource type and format from the object definition
    for (int x = 0; (x < _defObject->numResources) && !foundIt; x++) {
        if ((strcmp(resourceNumber, _defObject->resources[x].name) == 0) &&
            (wantedInstance == _defObject->resources[x].instance)) {
            type = _defObject->resources[x].type;
            format = _defObject->resources[x].format;
            foundIt = true;
        }
    }

    if (foundIt && (type == M2MResourceBase::STRING)) {
        success = setResourceValue((void *) &value,
                                   type,
                                   resourceNumber,
                                   wantedInstance,
                                   format);
    }

    return success;
}

// Get the value of a given resource in an object.
bool M2MObjectHelper::getResourceValue(int64_t *value,
                                       const char *resourceNumber,
                                       int wantedInstance)
{
    bool success = false;
    bool foundIt = false;
    M2MResourceBase::ResourceType type;

    // Find the resource type and format from the object definition
    for (int x = 0; (x < _defObject->numResources) && !foundIt; x++) {
        if ((strcmp(resourceNumber, _defObject->resources[x].name) == 0) &&
            (wantedInstance == _defObject->resources[x].instance)) {
            type = _defObject->resources[x].type;
            foundIt = true;
        }
    }

    // Get the value
    if (foundIt &&
       ((type == M2MResourceBase::INTEGER) ||
         type == M2MResourceBase::TIME)) {
        success = getResourceValue((void *) value,
                                   type,
                                   resourceNumber,
                                   wantedInstance);
    }

    return success;
}

// Get the value of a given resource in an object.
bool M2MObjectHelper::getResourceValue(float *value,
                                       const char *resourceNumber,
                                       int wantedInstance)
{
    bool success = false;
    bool foundIt = false;
    M2MResourceBase::ResourceType type;

    // Find the resource type and format from the object definition
    for (int x = 0; (x < _defObject->numResources) && !foundIt; x++) {
        if ((strcmp(resourceNumber, _defObject->resources[x].name) == 0) &&
            (wantedInstance == _defObject->resources[x].instance)) {
            type = _defObject->resources[x].type;
            foundIt = true;
        }
    }

    // Get the value
    if (foundIt && (type == M2MResourceBase::FLOAT)) {
        success = getResourceValue((void *) value,
                                   type,
                                   resourceNumber,
                                   wantedInstance);
    }

    return success;
}

// Get the value of a given resource in an object.
bool M2MObjectHelper::getResourceValue(bool *value,
                                       const char *resourceNumber,
                                       int wantedInstance)
{
    bool success = false;
    bool foundIt = false;
    M2MResourceBase::ResourceType type;

    // Find the resource type and format from the object definition
    for (int x = 0; (x < _defObject->numResources) && !foundIt; x++) {
        if ((strcmp(resourceNumber, _defObject->resources[x].name) == 0) &&
            (wantedInstance == _defObject->resources[x].instance)) {
            type = _defObject->resources[x].type;
            foundIt = true;
        }
    }

    // Get the value
    if (foundIt && (type == M2MResourceBase::BOOLEAN)) {
        success = getResourceValue((void *) value,
                                   type,
                                   resourceNumber,
                                   wantedInstance);
    }

    return success;
}

// Get the value of a given resource in an object.
bool M2MObjectHelper::getResourceValue(char *value,
                                       unsigned int len,
                                       const char *resourceNumber,
                                       int wantedInstance)
{
    bool success = false;
    bool foundIt = false;
    M2MResourceBase::ResourceType type;
    String *str = new String();

    // Find the resource type and format from the object definition
    for (int x = 0; (x < _defObject->numResources) && !foundIt; x++) {
        if ((strcmp(resourceNumber, _defObject->resources[x].name) == 0) &&
            (wantedInstance == _defObject->resources[x].instance)) {
            type = _defObject->resources[x].type;
            foundIt = true;
        }
    }

    // Get the value
    if (foundIt && (type == M2MResourceBase::STRING)) {
        success = getResourceValue((void *) str, type, resourceNumber, wantedInstance);
        // Convert the string
        if (success) {
            if (len > 0) {
                if (str->size() > len - 1) { // -1 for terminator
                    str->resize(len - 1);
                }
                memcpy(value, str->c_str(), str->size());
                *(value + str->size()) = 0; // Add terminator
            }
        }
    }

    return success;
}

// Get the value of a given resource in an object.
bool M2MObjectHelper::getResourceValue(String *value,
                                       const char *resourceNumber,
                                       int wantedInstance)
{
    bool success = false;
    bool foundIt = false;
    M2MResourceBase::ResourceType type;

    // Find the resource type and format from the object definition
    for (int x = 0; (x < _defObject->numResources) && !foundIt; x++) {
        if ((strcmp(resourceNumber, _defObject->resources[x].name) == 0) &&
            (wantedInstance == _defObject->resources[x].instance)) {
            type = _defObject->resources[x].type;
            foundIt = true;
        }
    }

    // Get the value
    if (foundIt && (type == M2MResourceBase::STRING)) {
        success = getResourceValue((void *) value,
                                   type,
                                   resourceNumber,
                                   wantedInstance);
    }

    return success;
}

// Return this object.
M2MObject *M2MObjectHelper::getObject()
{
    return _object;
}

/**********************************************************************
 * PROTECTED METHODS
 **********************************************************************/

// Constructor (private so that this can only be sub-classed).
M2MObjectHelper::M2MObjectHelper(const DefObject *defObject,
                                 value_updated_callback valueUpdatedCallback,
                                 M2MObject *object,
                                 bool debugOn)
{
    _debugOn = debugOn;
    _defObject = defObject;
    _object = object;
    _valueUpdatedCallback = valueUpdatedCallback;
}

/**********************************************************************
 * PRIVATE METHODS
 **********************************************************************/

// Set the value of a given resource in an object.
bool M2MObjectHelper::setResourceValue(const void *value,
                                       M2MResourceBase::ResourceType type,
                                       const char *resourceNumber,
                                       int wantedInstance,
                                       const char *format)
{
    bool success = false;
    M2MObjectInstance *objectInstance;
    M2MResource *resource;
    M2MResourceInstance *resourceInstance = NULL;
    bool foundIt = false;
    int64_t valueInt64;
    char buffer[32];
    int length;

    if (_object != NULL) {
        objectInstance = _object->object_instance(_defObject->instance);
        if (objectInstance != NULL) {
            resource = objectInstance->resource(resourceNumber);
            if (resource != NULL) {

                printfLog("M2MObjectHelper: setting value of resource \"%s\", instance %d (-1 == single instance), in object \"%s\".\n",
                          resourceNumber, wantedInstance, _object->name());

                // If this is a multi-instance resource, get the resource instance
                if (resource->supports_multiple_instances()) {
                    resourceInstance = resource->resource_instance(wantedInstance);
                    if (resourceInstance != NULL) {
                        foundIt = true;
                    }
                } else {
                    foundIt = true;
                }

                if (foundIt) {
                    switch (type) {
                        case M2MResourceBase::STRING:
                            printfLog("M2MObjectHelper:   STRING resource set to \"%s\".\n", (*((String *) value)).c_str());
                            if (resourceInstance != NULL) {
                                success = resourceInstance->set_value((const uint8_t *) (*((const String *) value)).c_str(), (*((const String *) value)).size());
                            } else {
                                success = resource->set_value((const uint8_t *) (*((const String *) value)).c_str(), (*((const String *) value)).size());
                            }
                            break;
                        case M2MResourceBase::INTEGER:
                        case M2MResourceBase::TIME:
                            valueInt64 = *((int64_t *) value);
                            printfLog("M2MObjectHelper:   INTEGER or TIME resource set to %lld.\n", valueInt64);
                            if (resourceInstance != NULL) {
                                success = resourceInstance->set_value(valueInt64);
                            } else {
                                success = resource->set_value(valueInt64);
                            }
                            break;
                        case M2MResourceBase::BOOLEAN:
                            valueInt64 = *((bool *) value);
                            printfLog("M2MObjectHelper:   BOOLEAN resource set to %lld.\n", valueInt64);
                            if (resourceInstance != NULL) {
                                success = resourceInstance->set_value(valueInt64);
                            } else {
                                success = resource->set_value(valueInt64);
                            }
                            break;
                        case M2MResourceBase::FLOAT:
                            if (format == NULL) {
                                format = "%f";
                            }
                            length = snprintf(buffer, sizeof(buffer), format, *((float *) value));
                            printfLog("M2MObjectHelper:   FLOAT resource set to %f (\"%*s\", the format string being \"%s\").\n",
                                      *((float *) value), length, buffer, format);
                            if (resourceInstance != NULL) {
                                success = resourceInstance->set_value((uint8_t *) buffer, length);
                            } else {
                                success = resource->set_value((uint8_t *) buffer, length);
                            }
                            break;
                        case M2MResourceBase::OBJLINK:
                        case M2MResourceBase::OPAQUE:
                            printfLog("M2MObjectHelper:   don't know how to handle resource type %d (OBJLINK or OPAQUE).\n", type);
                            break;
                        default:
                            printfLog("M2MObjectHelper:   unknown resource type %d.\n", type);
                            break;
                    }
                } else {
                    printfLog("M2MObjectHelper: unable to find resource \"%s\", instance %d, in object \"%s\".\n",
                              resourceNumber, wantedInstance, _object->name());
                }
            } else {
                printfLog("M2MObjectHelper: unable to find resource \"%s\" of object \"%s\".\n",
                          resourceNumber, _object->name());
            }
        } else {
            printfLog("M2MObjectHelper: unable to get instance of object \"%s\".\n", _object->name());
        }
    } else {
        printfLog("M2MObjectHelper: object is NULL.\n");
    }

    return success;
}

// Get the value of a given resource in an object.
bool M2MObjectHelper::getResourceValue(void *value,
                                       M2MResourceBase::ResourceType type,
                                       const char *resourceNumber,
                                       int wantedInstance)
{
    bool success = false;
    M2MObjectInstance *objectInstance;
    M2MResource *resource;
    M2MResourceInstance *resourceInstance = NULL;
    bool foundIt = false;
    int64_t localValue;
    String str;

    if (_object != NULL) {
        objectInstance = _object->object_instance(_defObject->instance);
        if (objectInstance != NULL) {
            resource = objectInstance->resource(resourceNumber);
            if (resource != NULL) {
                printfLog("M2MObjectHelper: getting value of resource \"%s\", instance %d (-1 == single instance), from object \"%s\".\n",
                          resourceNumber, wantedInstance, _object->name());

                // If this is a multi-instance resource, get the resource instance
                if (resource->supports_multiple_instances()) {
                    resourceInstance = resource->resource_instance(wantedInstance);
                    if (resourceInstance != NULL) {
                        foundIt = true;
                    }
                } else {
                    foundIt = true;
                }

                if (foundIt) {
                    switch (type) {
                        case M2MResourceBase::STRING:
                            if (resourceInstance != NULL) {
                                *(String *) value = resourceInstance->get_value_string();
                            } else {
                                *(String *) value = resource->get_value_string();
                            }
                            printfLog("M2MObjectHelper:   STRING resource value is \"%s\".\n", (*((String *) value)).c_str());
                            success = true;
                            break;
                        case M2MResourceBase::INTEGER:
                        case M2MResourceBase::TIME:
                            if (resourceInstance != NULL) {
                                localValue = resourceInstance->get_value_int();
                            } else {
                                localValue = resource->get_value_int();
                            }
                            *((int64_t *) value) = localValue;
                            printfLog("M2MObjectHelper:   INTEGER or TIME resource value is %lld.\n", *((int64_t *) value));
                            success = true;
                            break;
                        case M2MResourceBase::BOOLEAN:
                            if (resourceInstance != NULL) {
                                localValue = resourceInstance->get_value_int();
                            } else {
                                localValue = resource->get_value_int();
                            }
                            *(bool *) value = ((bool) localValue != false);
                            printfLog("M2MObjectHelper:   BOOLEAN resource value is %d.\n", *((bool *) value));
                            success = true;
                            break;
                        case M2MResourceBase::FLOAT:
                            if (resourceInstance != NULL) {
                                str = resourceInstance->get_value_string();
                            } else {
                                str = resource->get_value_string();
                            }
                            sscanf(str.c_str(), "%f", (float *) value);
                            printfLog("M2MObjectHelper:   FLOAT resource value is %f (\"%s\").\n", *((float *) value), str.c_str());
                            success = true;
                            break;
                        case M2MResourceBase::OBJLINK:
                        case M2MResourceBase::OPAQUE:
                            printfLog("M2MObjectHelper:   don't know how to handle resource type %d (OBJLINK or OPAQUE).\n", type);
                            break;
                        default:
                            printfLog("M2MObjectHelper:   unknown resource type %d.\n", type);
                            break;
                    }
                } else {
                    printfLog("M2MObjectHelper: unable to find resource \"%s\", instance %d, in object \"%s\".\n",
                              resourceNumber, wantedInstance, _object->name());
                }
            } else {
                printfLog("M2MObjectHelper: unable to find resource \"%s\" of object \"%s\".\n",
                          resourceNumber, _object->name());
            }
        } else {
            printfLog("M2MObjectHelper: unable to get instance of object \"%s\".\n", _object->name());
        }
    } else {
        printfLog("M2MObjectHelper: object is NULL.\n");
    }

    return success;
}

// End of file
