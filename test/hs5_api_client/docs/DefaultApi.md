# hs5_api.DefaultApi

All URIs are relative to *http://localhost*

Method | HTTP request | Description
------------- | ------------- | -------------
[**add_access_key**](DefaultApi.md#add_access_key) | **POST** /api-v1-b64be512-4b03-4028-a589-13931942e205/addAccessKey | 
[**add_bucket**](DefaultApi.md#add_bucket) | **POST** /api-v1-b64be512-4b03-4028-a589-13931942e205/addBucket | 
[**add_policy**](DefaultApi.md#add_policy) | **POST** /api-v1-b64be512-4b03-4028-a589-13931942e205/addPolicy | 
[**add_role**](DefaultApi.md#add_role) | **POST** /api-v1-b64be512-4b03-4028-a589-13931942e205/addRole | 
[**add_role_policy**](DefaultApi.md#add_role_policy) | **POST** /api-v1-b64be512-4b03-4028-a589-13931942e205/addRolePolicy | 
[**add_user**](DefaultApi.md#add_user) | **POST** /api-v1-b64be512-4b03-4028-a589-13931942e205/addUser | 
[**add_user_role**](DefaultApi.md#add_user_role) | **POST** /api-v1-b64be512-4b03-4028-a589-13931942e205/addUserRole | 
[**change_password**](DefaultApi.md#change_password) | **POST** /api-v1-b64be512-4b03-4028-a589-13931942e205/changePassword | 
[**change_policy**](DefaultApi.md#change_policy) | **POST** /api-v1-b64be512-4b03-4028-a589-13931942e205/changePolicy | 
[**delete_bucket**](DefaultApi.md#delete_bucket) | **POST** /api-v1-b64be512-4b03-4028-a589-13931942e205/deleteBucket | 
[**list**](DefaultApi.md#list) | **POST** /api-v1-b64be512-4b03-4028-a589-13931942e205/list | 
[**list_access_keys**](DefaultApi.md#list_access_keys) | **POST** /api-v1-b64be512-4b03-4028-a589-13931942e205/listAccessKeys | 
[**list_policies**](DefaultApi.md#list_policies) | **POST** /api-v1-b64be512-4b03-4028-a589-13931942e205/listPolicies | 
[**list_role_policies**](DefaultApi.md#list_role_policies) | **POST** /api-v1-b64be512-4b03-4028-a589-13931942e205/listRolePolicies | 
[**list_roles**](DefaultApi.md#list_roles) | **POST** /api-v1-b64be512-4b03-4028-a589-13931942e205/listRoles | 
[**list_user_roles**](DefaultApi.md#list_user_roles) | **POST** /api-v1-b64be512-4b03-4028-a589-13931942e205/listUserRoles | 
[**list_users**](DefaultApi.md#list_users) | **POST** /api-v1-b64be512-4b03-4028-a589-13931942e205/listUsers | 
[**login**](DefaultApi.md#login) | **POST** /api-v1-b64be512-4b03-4028-a589-13931942e205/login | 
[**logout**](DefaultApi.md#logout) | **POST** /api-v1-b64be512-4b03-4028-a589-13931942e205/logout | 
[**remove_access_key**](DefaultApi.md#remove_access_key) | **POST** /api-v1-b64be512-4b03-4028-a589-13931942e205/removeAccessKey | 
[**remove_policy**](DefaultApi.md#remove_policy) | **POST** /api-v1-b64be512-4b03-4028-a589-13931942e205/removePolicy | 
[**remove_role**](DefaultApi.md#remove_role) | **POST** /api-v1-b64be512-4b03-4028-a589-13931942e205/removeRole | 
[**remove_role_policy**](DefaultApi.md#remove_role_policy) | **POST** /api-v1-b64be512-4b03-4028-a589-13931942e205/removeRolePolicy | 
[**remove_user**](DefaultApi.md#remove_user) | **POST** /api-v1-b64be512-4b03-4028-a589-13931942e205/removeUser | 
[**remove_user_role**](DefaultApi.md#remove_user_role) | **POST** /api-v1-b64be512-4b03-4028-a589-13931942e205/removeUserRole | 
[**session_check**](DefaultApi.md#session_check) | **POST** /api-v1-b64be512-4b03-4028-a589-13931942e205/sessionCheck | 


# **add_access_key**
> AddAccessKeyResp add_access_key(add_access_key_params=add_access_key_params)

Add access key

### Example

* Api Key Authentication (cookieAuth):
* Bearer Authentication (bearerAuth):

```python
import hs5_api
from hs5_api.models.add_access_key_params import AddAccessKeyParams
from hs5_api.models.add_access_key_resp import AddAccessKeyResp
from hs5_api.rest import ApiException
from pprint import pprint

# Defining the host is optional and defaults to http://localhost
# See configuration.py for a list of all supported configuration parameters.
configuration = hs5_api.Configuration(
    host = "http://localhost"
)

# The client must configure the authentication and authorization parameters
# in accordance with the API server security policy.
# Examples for each auth method are provided below, use the example that
# satisfies your auth use case.

# Configure API key authorization: cookieAuth
configuration.api_key['cookieAuth'] = os.environ["API_KEY"]

# Uncomment below to setup prefix (e.g. Bearer) for API key, if needed
# configuration.api_key_prefix['cookieAuth'] = 'Bearer'

# Configure Bearer authorization: bearerAuth
configuration = hs5_api.Configuration(
    access_token = os.environ["BEARER_TOKEN"]
)

# Enter a context with an instance of the API client
with hs5_api.ApiClient(configuration) as api_client:
    # Create an instance of the API class
    api_instance = hs5_api.DefaultApi(api_client)
    add_access_key_params = hs5_api.AddAccessKeyParams() # AddAccessKeyParams |  (optional)

    try:
        api_response = api_instance.add_access_key(add_access_key_params=add_access_key_params)
        print("The response of DefaultApi->add_access_key:\n")
        pprint(api_response)
    except Exception as e:
        print("Exception when calling DefaultApi->add_access_key: %s\n" % e)
```



### Parameters


Name | Type | Description  | Notes
------------- | ------------- | ------------- | -------------
 **add_access_key_params** | [**AddAccessKeyParams**](AddAccessKeyParams.md)|  | [optional] 

### Return type

[**AddAccessKeyResp**](AddAccessKeyResp.md)

### Authorization

[cookieAuth](../README.md#cookieAuth), [bearerAuth](../README.md#bearerAuth)

### HTTP request headers

 - **Content-Type**: application/json
 - **Accept**: application/json

### HTTP response details

| Status code | Description | Response headers |
|-------------|-------------|------------------|
**200** | OK |  -  |
**400** | Error |  -  |

[[Back to top]](#) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to Model list]](../README.md#documentation-for-models) [[Back to README]](../README.md)

# **add_bucket**
> AddBucketResp add_bucket(add_bucket_params=add_bucket_params)

Add new bucket

### Example

* Api Key Authentication (cookieAuth):
* Bearer Authentication (bearerAuth):

```python
import hs5_api
from hs5_api.models.add_bucket_params import AddBucketParams
from hs5_api.models.add_bucket_resp import AddBucketResp
from hs5_api.rest import ApiException
from pprint import pprint

# Defining the host is optional and defaults to http://localhost
# See configuration.py for a list of all supported configuration parameters.
configuration = hs5_api.Configuration(
    host = "http://localhost"
)

# The client must configure the authentication and authorization parameters
# in accordance with the API server security policy.
# Examples for each auth method are provided below, use the example that
# satisfies your auth use case.

# Configure API key authorization: cookieAuth
configuration.api_key['cookieAuth'] = os.environ["API_KEY"]

# Uncomment below to setup prefix (e.g. Bearer) for API key, if needed
# configuration.api_key_prefix['cookieAuth'] = 'Bearer'

# Configure Bearer authorization: bearerAuth
configuration = hs5_api.Configuration(
    access_token = os.environ["BEARER_TOKEN"]
)

# Enter a context with an instance of the API client
with hs5_api.ApiClient(configuration) as api_client:
    # Create an instance of the API class
    api_instance = hs5_api.DefaultApi(api_client)
    add_bucket_params = hs5_api.AddBucketParams() # AddBucketParams |  (optional)

    try:
        api_response = api_instance.add_bucket(add_bucket_params=add_bucket_params)
        print("The response of DefaultApi->add_bucket:\n")
        pprint(api_response)
    except Exception as e:
        print("Exception when calling DefaultApi->add_bucket: %s\n" % e)
```



### Parameters


Name | Type | Description  | Notes
------------- | ------------- | ------------- | -------------
 **add_bucket_params** | [**AddBucketParams**](AddBucketParams.md)|  | [optional] 

### Return type

[**AddBucketResp**](AddBucketResp.md)

### Authorization

[cookieAuth](../README.md#cookieAuth), [bearerAuth](../README.md#bearerAuth)

### HTTP request headers

 - **Content-Type**: application/json
 - **Accept**: application/json

### HTTP response details

| Status code | Description | Response headers |
|-------------|-------------|------------------|
**200** | OK |  -  |
**400** | Error |  -  |

[[Back to top]](#) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to Model list]](../README.md#documentation-for-models) [[Back to README]](../README.md)

# **add_policy**
> AddPolicyResp add_policy(add_policy_params=add_policy_params)

Add policy

### Example

* Api Key Authentication (cookieAuth):
* Bearer Authentication (bearerAuth):

```python
import hs5_api
from hs5_api.models.add_policy_params import AddPolicyParams
from hs5_api.models.add_policy_resp import AddPolicyResp
from hs5_api.rest import ApiException
from pprint import pprint

# Defining the host is optional and defaults to http://localhost
# See configuration.py for a list of all supported configuration parameters.
configuration = hs5_api.Configuration(
    host = "http://localhost"
)

# The client must configure the authentication and authorization parameters
# in accordance with the API server security policy.
# Examples for each auth method are provided below, use the example that
# satisfies your auth use case.

# Configure API key authorization: cookieAuth
configuration.api_key['cookieAuth'] = os.environ["API_KEY"]

# Uncomment below to setup prefix (e.g. Bearer) for API key, if needed
# configuration.api_key_prefix['cookieAuth'] = 'Bearer'

# Configure Bearer authorization: bearerAuth
configuration = hs5_api.Configuration(
    access_token = os.environ["BEARER_TOKEN"]
)

# Enter a context with an instance of the API client
with hs5_api.ApiClient(configuration) as api_client:
    # Create an instance of the API class
    api_instance = hs5_api.DefaultApi(api_client)
    add_policy_params = hs5_api.AddPolicyParams() # AddPolicyParams |  (optional)

    try:
        api_response = api_instance.add_policy(add_policy_params=add_policy_params)
        print("The response of DefaultApi->add_policy:\n")
        pprint(api_response)
    except Exception as e:
        print("Exception when calling DefaultApi->add_policy: %s\n" % e)
```



### Parameters


Name | Type | Description  | Notes
------------- | ------------- | ------------- | -------------
 **add_policy_params** | [**AddPolicyParams**](AddPolicyParams.md)|  | [optional] 

### Return type

[**AddPolicyResp**](AddPolicyResp.md)

### Authorization

[cookieAuth](../README.md#cookieAuth), [bearerAuth](../README.md#bearerAuth)

### HTTP request headers

 - **Content-Type**: application/json
 - **Accept**: application/json

### HTTP response details

| Status code | Description | Response headers |
|-------------|-------------|------------------|
**200** | OK |  -  |
**400** | Error |  -  |

[[Back to top]](#) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to Model list]](../README.md#documentation-for-models) [[Back to README]](../README.md)

# **add_role**
> AddRoleResp add_role(add_role_params=add_role_params)

Add role

### Example

* Api Key Authentication (cookieAuth):
* Bearer Authentication (bearerAuth):

```python
import hs5_api
from hs5_api.models.add_role_params import AddRoleParams
from hs5_api.models.add_role_resp import AddRoleResp
from hs5_api.rest import ApiException
from pprint import pprint

# Defining the host is optional and defaults to http://localhost
# See configuration.py for a list of all supported configuration parameters.
configuration = hs5_api.Configuration(
    host = "http://localhost"
)

# The client must configure the authentication and authorization parameters
# in accordance with the API server security policy.
# Examples for each auth method are provided below, use the example that
# satisfies your auth use case.

# Configure API key authorization: cookieAuth
configuration.api_key['cookieAuth'] = os.environ["API_KEY"]

# Uncomment below to setup prefix (e.g. Bearer) for API key, if needed
# configuration.api_key_prefix['cookieAuth'] = 'Bearer'

# Configure Bearer authorization: bearerAuth
configuration = hs5_api.Configuration(
    access_token = os.environ["BEARER_TOKEN"]
)

# Enter a context with an instance of the API client
with hs5_api.ApiClient(configuration) as api_client:
    # Create an instance of the API class
    api_instance = hs5_api.DefaultApi(api_client)
    add_role_params = hs5_api.AddRoleParams() # AddRoleParams |  (optional)

    try:
        api_response = api_instance.add_role(add_role_params=add_role_params)
        print("The response of DefaultApi->add_role:\n")
        pprint(api_response)
    except Exception as e:
        print("Exception when calling DefaultApi->add_role: %s\n" % e)
```



### Parameters


Name | Type | Description  | Notes
------------- | ------------- | ------------- | -------------
 **add_role_params** | [**AddRoleParams**](AddRoleParams.md)|  | [optional] 

### Return type

[**AddRoleResp**](AddRoleResp.md)

### Authorization

[cookieAuth](../README.md#cookieAuth), [bearerAuth](../README.md#bearerAuth)

### HTTP request headers

 - **Content-Type**: application/json
 - **Accept**: application/json

### HTTP response details

| Status code | Description | Response headers |
|-------------|-------------|------------------|
**200** | OK |  -  |
**400** | Error |  -  |

[[Back to top]](#) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to Model list]](../README.md#documentation-for-models) [[Back to README]](../README.md)

# **add_role_policy**
> AddRolePolicyResp add_role_policy(add_role_policy_params=add_role_policy_params)

Add role policy

### Example

* Api Key Authentication (cookieAuth):
* Bearer Authentication (bearerAuth):

```python
import hs5_api
from hs5_api.models.add_role_policy_params import AddRolePolicyParams
from hs5_api.models.add_role_policy_resp import AddRolePolicyResp
from hs5_api.rest import ApiException
from pprint import pprint

# Defining the host is optional and defaults to http://localhost
# See configuration.py for a list of all supported configuration parameters.
configuration = hs5_api.Configuration(
    host = "http://localhost"
)

# The client must configure the authentication and authorization parameters
# in accordance with the API server security policy.
# Examples for each auth method are provided below, use the example that
# satisfies your auth use case.

# Configure API key authorization: cookieAuth
configuration.api_key['cookieAuth'] = os.environ["API_KEY"]

# Uncomment below to setup prefix (e.g. Bearer) for API key, if needed
# configuration.api_key_prefix['cookieAuth'] = 'Bearer'

# Configure Bearer authorization: bearerAuth
configuration = hs5_api.Configuration(
    access_token = os.environ["BEARER_TOKEN"]
)

# Enter a context with an instance of the API client
with hs5_api.ApiClient(configuration) as api_client:
    # Create an instance of the API class
    api_instance = hs5_api.DefaultApi(api_client)
    add_role_policy_params = hs5_api.AddRolePolicyParams() # AddRolePolicyParams |  (optional)

    try:
        api_response = api_instance.add_role_policy(add_role_policy_params=add_role_policy_params)
        print("The response of DefaultApi->add_role_policy:\n")
        pprint(api_response)
    except Exception as e:
        print("Exception when calling DefaultApi->add_role_policy: %s\n" % e)
```



### Parameters


Name | Type | Description  | Notes
------------- | ------------- | ------------- | -------------
 **add_role_policy_params** | [**AddRolePolicyParams**](AddRolePolicyParams.md)|  | [optional] 

### Return type

[**AddRolePolicyResp**](AddRolePolicyResp.md)

### Authorization

[cookieAuth](../README.md#cookieAuth), [bearerAuth](../README.md#bearerAuth)

### HTTP request headers

 - **Content-Type**: application/json
 - **Accept**: application/json

### HTTP response details

| Status code | Description | Response headers |
|-------------|-------------|------------------|
**200** | OK |  -  |
**400** | Error |  -  |

[[Back to top]](#) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to Model list]](../README.md#documentation-for-models) [[Back to README]](../README.md)

# **add_user**
> AddUserResp add_user(add_user_params=add_user_params)

Adds a user

### Example

* Api Key Authentication (cookieAuth):
* Bearer Authentication (bearerAuth):

```python
import hs5_api
from hs5_api.models.add_user_params import AddUserParams
from hs5_api.models.add_user_resp import AddUserResp
from hs5_api.rest import ApiException
from pprint import pprint

# Defining the host is optional and defaults to http://localhost
# See configuration.py for a list of all supported configuration parameters.
configuration = hs5_api.Configuration(
    host = "http://localhost"
)

# The client must configure the authentication and authorization parameters
# in accordance with the API server security policy.
# Examples for each auth method are provided below, use the example that
# satisfies your auth use case.

# Configure API key authorization: cookieAuth
configuration.api_key['cookieAuth'] = os.environ["API_KEY"]

# Uncomment below to setup prefix (e.g. Bearer) for API key, if needed
# configuration.api_key_prefix['cookieAuth'] = 'Bearer'

# Configure Bearer authorization: bearerAuth
configuration = hs5_api.Configuration(
    access_token = os.environ["BEARER_TOKEN"]
)

# Enter a context with an instance of the API client
with hs5_api.ApiClient(configuration) as api_client:
    # Create an instance of the API class
    api_instance = hs5_api.DefaultApi(api_client)
    add_user_params = hs5_api.AddUserParams() # AddUserParams |  (optional)

    try:
        api_response = api_instance.add_user(add_user_params=add_user_params)
        print("The response of DefaultApi->add_user:\n")
        pprint(api_response)
    except Exception as e:
        print("Exception when calling DefaultApi->add_user: %s\n" % e)
```



### Parameters


Name | Type | Description  | Notes
------------- | ------------- | ------------- | -------------
 **add_user_params** | [**AddUserParams**](AddUserParams.md)|  | [optional] 

### Return type

[**AddUserResp**](AddUserResp.md)

### Authorization

[cookieAuth](../README.md#cookieAuth), [bearerAuth](../README.md#bearerAuth)

### HTTP request headers

 - **Content-Type**: application/json
 - **Accept**: application/json

### HTTP response details

| Status code | Description | Response headers |
|-------------|-------------|------------------|
**200** | OK |  -  |
**400** | Error |  -  |

[[Back to top]](#) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to Model list]](../README.md#documentation-for-models) [[Back to README]](../README.md)

# **add_user_role**
> AddUserRoleResp add_user_role(add_user_role_params=add_user_role_params)

Add user role

### Example

* Api Key Authentication (cookieAuth):
* Bearer Authentication (bearerAuth):

```python
import hs5_api
from hs5_api.models.add_user_role_params import AddUserRoleParams
from hs5_api.models.add_user_role_resp import AddUserRoleResp
from hs5_api.rest import ApiException
from pprint import pprint

# Defining the host is optional and defaults to http://localhost
# See configuration.py for a list of all supported configuration parameters.
configuration = hs5_api.Configuration(
    host = "http://localhost"
)

# The client must configure the authentication and authorization parameters
# in accordance with the API server security policy.
# Examples for each auth method are provided below, use the example that
# satisfies your auth use case.

# Configure API key authorization: cookieAuth
configuration.api_key['cookieAuth'] = os.environ["API_KEY"]

# Uncomment below to setup prefix (e.g. Bearer) for API key, if needed
# configuration.api_key_prefix['cookieAuth'] = 'Bearer'

# Configure Bearer authorization: bearerAuth
configuration = hs5_api.Configuration(
    access_token = os.environ["BEARER_TOKEN"]
)

# Enter a context with an instance of the API client
with hs5_api.ApiClient(configuration) as api_client:
    # Create an instance of the API class
    api_instance = hs5_api.DefaultApi(api_client)
    add_user_role_params = hs5_api.AddUserRoleParams() # AddUserRoleParams |  (optional)

    try:
        api_response = api_instance.add_user_role(add_user_role_params=add_user_role_params)
        print("The response of DefaultApi->add_user_role:\n")
        pprint(api_response)
    except Exception as e:
        print("Exception when calling DefaultApi->add_user_role: %s\n" % e)
```



### Parameters


Name | Type | Description  | Notes
------------- | ------------- | ------------- | -------------
 **add_user_role_params** | [**AddUserRoleParams**](AddUserRoleParams.md)|  | [optional] 

### Return type

[**AddUserRoleResp**](AddUserRoleResp.md)

### Authorization

[cookieAuth](../README.md#cookieAuth), [bearerAuth](../README.md#bearerAuth)

### HTTP request headers

 - **Content-Type**: application/json
 - **Accept**: application/json

### HTTP response details

| Status code | Description | Response headers |
|-------------|-------------|------------------|
**200** | OK |  -  |
**400** | Error |  -  |

[[Back to top]](#) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to Model list]](../README.md#documentation-for-models) [[Back to README]](../README.md)

# **change_password**
> ChangePasswordResp change_password(change_password_params=change_password_params)

Change password

### Example

* Api Key Authentication (cookieAuth):
* Bearer Authentication (bearerAuth):

```python
import hs5_api
from hs5_api.models.change_password_params import ChangePasswordParams
from hs5_api.models.change_password_resp import ChangePasswordResp
from hs5_api.rest import ApiException
from pprint import pprint

# Defining the host is optional and defaults to http://localhost
# See configuration.py for a list of all supported configuration parameters.
configuration = hs5_api.Configuration(
    host = "http://localhost"
)

# The client must configure the authentication and authorization parameters
# in accordance with the API server security policy.
# Examples for each auth method are provided below, use the example that
# satisfies your auth use case.

# Configure API key authorization: cookieAuth
configuration.api_key['cookieAuth'] = os.environ["API_KEY"]

# Uncomment below to setup prefix (e.g. Bearer) for API key, if needed
# configuration.api_key_prefix['cookieAuth'] = 'Bearer'

# Configure Bearer authorization: bearerAuth
configuration = hs5_api.Configuration(
    access_token = os.environ["BEARER_TOKEN"]
)

# Enter a context with an instance of the API client
with hs5_api.ApiClient(configuration) as api_client:
    # Create an instance of the API class
    api_instance = hs5_api.DefaultApi(api_client)
    change_password_params = hs5_api.ChangePasswordParams() # ChangePasswordParams |  (optional)

    try:
        api_response = api_instance.change_password(change_password_params=change_password_params)
        print("The response of DefaultApi->change_password:\n")
        pprint(api_response)
    except Exception as e:
        print("Exception when calling DefaultApi->change_password: %s\n" % e)
```



### Parameters


Name | Type | Description  | Notes
------------- | ------------- | ------------- | -------------
 **change_password_params** | [**ChangePasswordParams**](ChangePasswordParams.md)|  | [optional] 

### Return type

[**ChangePasswordResp**](ChangePasswordResp.md)

### Authorization

[cookieAuth](../README.md#cookieAuth), [bearerAuth](../README.md#bearerAuth)

### HTTP request headers

 - **Content-Type**: application/json
 - **Accept**: application/json

### HTTP response details

| Status code | Description | Response headers |
|-------------|-------------|------------------|
**200** | OK |  -  |
**400** | Error |  -  |

[[Back to top]](#) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to Model list]](../README.md#documentation-for-models) [[Back to README]](../README.md)

# **change_policy**
> ChangePolicyResp change_policy(change_policy_params=change_policy_params)

Change policy

### Example

* Api Key Authentication (cookieAuth):
* Bearer Authentication (bearerAuth):

```python
import hs5_api
from hs5_api.models.change_policy_params import ChangePolicyParams
from hs5_api.models.change_policy_resp import ChangePolicyResp
from hs5_api.rest import ApiException
from pprint import pprint

# Defining the host is optional and defaults to http://localhost
# See configuration.py for a list of all supported configuration parameters.
configuration = hs5_api.Configuration(
    host = "http://localhost"
)

# The client must configure the authentication and authorization parameters
# in accordance with the API server security policy.
# Examples for each auth method are provided below, use the example that
# satisfies your auth use case.

# Configure API key authorization: cookieAuth
configuration.api_key['cookieAuth'] = os.environ["API_KEY"]

# Uncomment below to setup prefix (e.g. Bearer) for API key, if needed
# configuration.api_key_prefix['cookieAuth'] = 'Bearer'

# Configure Bearer authorization: bearerAuth
configuration = hs5_api.Configuration(
    access_token = os.environ["BEARER_TOKEN"]
)

# Enter a context with an instance of the API client
with hs5_api.ApiClient(configuration) as api_client:
    # Create an instance of the API class
    api_instance = hs5_api.DefaultApi(api_client)
    change_policy_params = hs5_api.ChangePolicyParams() # ChangePolicyParams |  (optional)

    try:
        api_response = api_instance.change_policy(change_policy_params=change_policy_params)
        print("The response of DefaultApi->change_policy:\n")
        pprint(api_response)
    except Exception as e:
        print("Exception when calling DefaultApi->change_policy: %s\n" % e)
```



### Parameters


Name | Type | Description  | Notes
------------- | ------------- | ------------- | -------------
 **change_policy_params** | [**ChangePolicyParams**](ChangePolicyParams.md)|  | [optional] 

### Return type

[**ChangePolicyResp**](ChangePolicyResp.md)

### Authorization

[cookieAuth](../README.md#cookieAuth), [bearerAuth](../README.md#bearerAuth)

### HTTP request headers

 - **Content-Type**: application/json
 - **Accept**: application/json

### HTTP response details

| Status code | Description | Response headers |
|-------------|-------------|------------------|
**200** | OK |  -  |
**400** | Error |  -  |

[[Back to top]](#) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to Model list]](../README.md#documentation-for-models) [[Back to README]](../README.md)

# **delete_bucket**
> DeleteBucketResp delete_bucket(delete_bucket_params=delete_bucket_params)

Delete bucket

### Example

* Api Key Authentication (cookieAuth):
* Bearer Authentication (bearerAuth):

```python
import hs5_api
from hs5_api.models.delete_bucket_params import DeleteBucketParams
from hs5_api.models.delete_bucket_resp import DeleteBucketResp
from hs5_api.rest import ApiException
from pprint import pprint

# Defining the host is optional and defaults to http://localhost
# See configuration.py for a list of all supported configuration parameters.
configuration = hs5_api.Configuration(
    host = "http://localhost"
)

# The client must configure the authentication and authorization parameters
# in accordance with the API server security policy.
# Examples for each auth method are provided below, use the example that
# satisfies your auth use case.

# Configure API key authorization: cookieAuth
configuration.api_key['cookieAuth'] = os.environ["API_KEY"]

# Uncomment below to setup prefix (e.g. Bearer) for API key, if needed
# configuration.api_key_prefix['cookieAuth'] = 'Bearer'

# Configure Bearer authorization: bearerAuth
configuration = hs5_api.Configuration(
    access_token = os.environ["BEARER_TOKEN"]
)

# Enter a context with an instance of the API client
with hs5_api.ApiClient(configuration) as api_client:
    # Create an instance of the API class
    api_instance = hs5_api.DefaultApi(api_client)
    delete_bucket_params = hs5_api.DeleteBucketParams() # DeleteBucketParams |  (optional)

    try:
        api_response = api_instance.delete_bucket(delete_bucket_params=delete_bucket_params)
        print("The response of DefaultApi->delete_bucket:\n")
        pprint(api_response)
    except Exception as e:
        print("Exception when calling DefaultApi->delete_bucket: %s\n" % e)
```



### Parameters


Name | Type | Description  | Notes
------------- | ------------- | ------------- | -------------
 **delete_bucket_params** | [**DeleteBucketParams**](DeleteBucketParams.md)|  | [optional] 

### Return type

[**DeleteBucketResp**](DeleteBucketResp.md)

### Authorization

[cookieAuth](../README.md#cookieAuth), [bearerAuth](../README.md#bearerAuth)

### HTTP request headers

 - **Content-Type**: application/json
 - **Accept**: application/json

### HTTP response details

| Status code | Description | Response headers |
|-------------|-------------|------------------|
**200** | OK |  -  |
**400** | Error |  -  |

[[Back to top]](#) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to Model list]](../README.md#documentation-for-models) [[Back to README]](../README.md)

# **list**
> ListResp list(list_params=list_params)

List objects at location

### Example

* Api Key Authentication (cookieAuth):
* Bearer Authentication (bearerAuth):

```python
import hs5_api
from hs5_api.models.list_params import ListParams
from hs5_api.models.list_resp import ListResp
from hs5_api.rest import ApiException
from pprint import pprint

# Defining the host is optional and defaults to http://localhost
# See configuration.py for a list of all supported configuration parameters.
configuration = hs5_api.Configuration(
    host = "http://localhost"
)

# The client must configure the authentication and authorization parameters
# in accordance with the API server security policy.
# Examples for each auth method are provided below, use the example that
# satisfies your auth use case.

# Configure API key authorization: cookieAuth
configuration.api_key['cookieAuth'] = os.environ["API_KEY"]

# Uncomment below to setup prefix (e.g. Bearer) for API key, if needed
# configuration.api_key_prefix['cookieAuth'] = 'Bearer'

# Configure Bearer authorization: bearerAuth
configuration = hs5_api.Configuration(
    access_token = os.environ["BEARER_TOKEN"]
)

# Enter a context with an instance of the API client
with hs5_api.ApiClient(configuration) as api_client:
    # Create an instance of the API class
    api_instance = hs5_api.DefaultApi(api_client)
    list_params = hs5_api.ListParams() # ListParams |  (optional)

    try:
        api_response = api_instance.list(list_params=list_params)
        print("The response of DefaultApi->list:\n")
        pprint(api_response)
    except Exception as e:
        print("Exception when calling DefaultApi->list: %s\n" % e)
```



### Parameters


Name | Type | Description  | Notes
------------- | ------------- | ------------- | -------------
 **list_params** | [**ListParams**](ListParams.md)|  | [optional] 

### Return type

[**ListResp**](ListResp.md)

### Authorization

[cookieAuth](../README.md#cookieAuth), [bearerAuth](../README.md#bearerAuth)

### HTTP request headers

 - **Content-Type**: application/json
 - **Accept**: application/json

### HTTP response details

| Status code | Description | Response headers |
|-------------|-------------|------------------|
**200** | OK |  -  |
**400** | Error |  -  |

[[Back to top]](#) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to Model list]](../README.md#documentation-for-models) [[Back to README]](../README.md)

# **list_access_keys**
> ListAccessKeysResp list_access_keys(list_access_keys_params=list_access_keys_params)

List access keys

### Example

* Api Key Authentication (cookieAuth):
* Bearer Authentication (bearerAuth):

```python
import hs5_api
from hs5_api.models.list_access_keys_params import ListAccessKeysParams
from hs5_api.models.list_access_keys_resp import ListAccessKeysResp
from hs5_api.rest import ApiException
from pprint import pprint

# Defining the host is optional and defaults to http://localhost
# See configuration.py for a list of all supported configuration parameters.
configuration = hs5_api.Configuration(
    host = "http://localhost"
)

# The client must configure the authentication and authorization parameters
# in accordance with the API server security policy.
# Examples for each auth method are provided below, use the example that
# satisfies your auth use case.

# Configure API key authorization: cookieAuth
configuration.api_key['cookieAuth'] = os.environ["API_KEY"]

# Uncomment below to setup prefix (e.g. Bearer) for API key, if needed
# configuration.api_key_prefix['cookieAuth'] = 'Bearer'

# Configure Bearer authorization: bearerAuth
configuration = hs5_api.Configuration(
    access_token = os.environ["BEARER_TOKEN"]
)

# Enter a context with an instance of the API client
with hs5_api.ApiClient(configuration) as api_client:
    # Create an instance of the API class
    api_instance = hs5_api.DefaultApi(api_client)
    list_access_keys_params = hs5_api.ListAccessKeysParams() # ListAccessKeysParams |  (optional)

    try:
        api_response = api_instance.list_access_keys(list_access_keys_params=list_access_keys_params)
        print("The response of DefaultApi->list_access_keys:\n")
        pprint(api_response)
    except Exception as e:
        print("Exception when calling DefaultApi->list_access_keys: %s\n" % e)
```



### Parameters


Name | Type | Description  | Notes
------------- | ------------- | ------------- | -------------
 **list_access_keys_params** | [**ListAccessKeysParams**](ListAccessKeysParams.md)|  | [optional] 

### Return type

[**ListAccessKeysResp**](ListAccessKeysResp.md)

### Authorization

[cookieAuth](../README.md#cookieAuth), [bearerAuth](../README.md#bearerAuth)

### HTTP request headers

 - **Content-Type**: application/json
 - **Accept**: application/json

### HTTP response details

| Status code | Description | Response headers |
|-------------|-------------|------------------|
**200** | OK |  -  |
**400** | Error |  -  |

[[Back to top]](#) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to Model list]](../README.md#documentation-for-models) [[Back to README]](../README.md)

# **list_policies**
> ListPoliciesResp list_policies(list_policies_params=list_policies_params)

List policies

### Example

* Api Key Authentication (cookieAuth):
* Bearer Authentication (bearerAuth):

```python
import hs5_api
from hs5_api.models.list_policies_params import ListPoliciesParams
from hs5_api.models.list_policies_resp import ListPoliciesResp
from hs5_api.rest import ApiException
from pprint import pprint

# Defining the host is optional and defaults to http://localhost
# See configuration.py for a list of all supported configuration parameters.
configuration = hs5_api.Configuration(
    host = "http://localhost"
)

# The client must configure the authentication and authorization parameters
# in accordance with the API server security policy.
# Examples for each auth method are provided below, use the example that
# satisfies your auth use case.

# Configure API key authorization: cookieAuth
configuration.api_key['cookieAuth'] = os.environ["API_KEY"]

# Uncomment below to setup prefix (e.g. Bearer) for API key, if needed
# configuration.api_key_prefix['cookieAuth'] = 'Bearer'

# Configure Bearer authorization: bearerAuth
configuration = hs5_api.Configuration(
    access_token = os.environ["BEARER_TOKEN"]
)

# Enter a context with an instance of the API client
with hs5_api.ApiClient(configuration) as api_client:
    # Create an instance of the API class
    api_instance = hs5_api.DefaultApi(api_client)
    list_policies_params = hs5_api.ListPoliciesParams() # ListPoliciesParams |  (optional)

    try:
        api_response = api_instance.list_policies(list_policies_params=list_policies_params)
        print("The response of DefaultApi->list_policies:\n")
        pprint(api_response)
    except Exception as e:
        print("Exception when calling DefaultApi->list_policies: %s\n" % e)
```



### Parameters


Name | Type | Description  | Notes
------------- | ------------- | ------------- | -------------
 **list_policies_params** | [**ListPoliciesParams**](ListPoliciesParams.md)|  | [optional] 

### Return type

[**ListPoliciesResp**](ListPoliciesResp.md)

### Authorization

[cookieAuth](../README.md#cookieAuth), [bearerAuth](../README.md#bearerAuth)

### HTTP request headers

 - **Content-Type**: application/json
 - **Accept**: application/json

### HTTP response details

| Status code | Description | Response headers |
|-------------|-------------|------------------|
**200** | OK |  -  |
**400** | Error |  -  |

[[Back to top]](#) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to Model list]](../README.md#documentation-for-models) [[Back to README]](../README.md)

# **list_role_policies**
> ListRolePoliciesResp list_role_policies(list_role_policies_params=list_role_policies_params)

List role policies

### Example

* Api Key Authentication (cookieAuth):
* Bearer Authentication (bearerAuth):

```python
import hs5_api
from hs5_api.models.list_role_policies_params import ListRolePoliciesParams
from hs5_api.models.list_role_policies_resp import ListRolePoliciesResp
from hs5_api.rest import ApiException
from pprint import pprint

# Defining the host is optional and defaults to http://localhost
# See configuration.py for a list of all supported configuration parameters.
configuration = hs5_api.Configuration(
    host = "http://localhost"
)

# The client must configure the authentication and authorization parameters
# in accordance with the API server security policy.
# Examples for each auth method are provided below, use the example that
# satisfies your auth use case.

# Configure API key authorization: cookieAuth
configuration.api_key['cookieAuth'] = os.environ["API_KEY"]

# Uncomment below to setup prefix (e.g. Bearer) for API key, if needed
# configuration.api_key_prefix['cookieAuth'] = 'Bearer'

# Configure Bearer authorization: bearerAuth
configuration = hs5_api.Configuration(
    access_token = os.environ["BEARER_TOKEN"]
)

# Enter a context with an instance of the API client
with hs5_api.ApiClient(configuration) as api_client:
    # Create an instance of the API class
    api_instance = hs5_api.DefaultApi(api_client)
    list_role_policies_params = hs5_api.ListRolePoliciesParams() # ListRolePoliciesParams |  (optional)

    try:
        api_response = api_instance.list_role_policies(list_role_policies_params=list_role_policies_params)
        print("The response of DefaultApi->list_role_policies:\n")
        pprint(api_response)
    except Exception as e:
        print("Exception when calling DefaultApi->list_role_policies: %s\n" % e)
```



### Parameters


Name | Type | Description  | Notes
------------- | ------------- | ------------- | -------------
 **list_role_policies_params** | [**ListRolePoliciesParams**](ListRolePoliciesParams.md)|  | [optional] 

### Return type

[**ListRolePoliciesResp**](ListRolePoliciesResp.md)

### Authorization

[cookieAuth](../README.md#cookieAuth), [bearerAuth](../README.md#bearerAuth)

### HTTP request headers

 - **Content-Type**: application/json
 - **Accept**: application/json

### HTTP response details

| Status code | Description | Response headers |
|-------------|-------------|------------------|
**200** | OK |  -  |
**400** | Error |  -  |

[[Back to top]](#) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to Model list]](../README.md#documentation-for-models) [[Back to README]](../README.md)

# **list_roles**
> ListRolesResp list_roles(list_roles_params=list_roles_params)

List roles

### Example

* Api Key Authentication (cookieAuth):
* Bearer Authentication (bearerAuth):

```python
import hs5_api
from hs5_api.models.list_roles_params import ListRolesParams
from hs5_api.models.list_roles_resp import ListRolesResp
from hs5_api.rest import ApiException
from pprint import pprint

# Defining the host is optional and defaults to http://localhost
# See configuration.py for a list of all supported configuration parameters.
configuration = hs5_api.Configuration(
    host = "http://localhost"
)

# The client must configure the authentication and authorization parameters
# in accordance with the API server security policy.
# Examples for each auth method are provided below, use the example that
# satisfies your auth use case.

# Configure API key authorization: cookieAuth
configuration.api_key['cookieAuth'] = os.environ["API_KEY"]

# Uncomment below to setup prefix (e.g. Bearer) for API key, if needed
# configuration.api_key_prefix['cookieAuth'] = 'Bearer'

# Configure Bearer authorization: bearerAuth
configuration = hs5_api.Configuration(
    access_token = os.environ["BEARER_TOKEN"]
)

# Enter a context with an instance of the API client
with hs5_api.ApiClient(configuration) as api_client:
    # Create an instance of the API class
    api_instance = hs5_api.DefaultApi(api_client)
    list_roles_params = hs5_api.ListRolesParams() # ListRolesParams |  (optional)

    try:
        api_response = api_instance.list_roles(list_roles_params=list_roles_params)
        print("The response of DefaultApi->list_roles:\n")
        pprint(api_response)
    except Exception as e:
        print("Exception when calling DefaultApi->list_roles: %s\n" % e)
```



### Parameters


Name | Type | Description  | Notes
------------- | ------------- | ------------- | -------------
 **list_roles_params** | [**ListRolesParams**](ListRolesParams.md)|  | [optional] 

### Return type

[**ListRolesResp**](ListRolesResp.md)

### Authorization

[cookieAuth](../README.md#cookieAuth), [bearerAuth](../README.md#bearerAuth)

### HTTP request headers

 - **Content-Type**: application/json
 - **Accept**: application/json

### HTTP response details

| Status code | Description | Response headers |
|-------------|-------------|------------------|
**200** | OK |  -  |
**400** | Error |  -  |

[[Back to top]](#) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to Model list]](../README.md#documentation-for-models) [[Back to README]](../README.md)

# **list_user_roles**
> ListUserRolesResp list_user_roles(list_user_roles_params=list_user_roles_params)

List user roles

### Example

* Api Key Authentication (cookieAuth):
* Bearer Authentication (bearerAuth):

```python
import hs5_api
from hs5_api.models.list_user_roles_params import ListUserRolesParams
from hs5_api.models.list_user_roles_resp import ListUserRolesResp
from hs5_api.rest import ApiException
from pprint import pprint

# Defining the host is optional and defaults to http://localhost
# See configuration.py for a list of all supported configuration parameters.
configuration = hs5_api.Configuration(
    host = "http://localhost"
)

# The client must configure the authentication and authorization parameters
# in accordance with the API server security policy.
# Examples for each auth method are provided below, use the example that
# satisfies your auth use case.

# Configure API key authorization: cookieAuth
configuration.api_key['cookieAuth'] = os.environ["API_KEY"]

# Uncomment below to setup prefix (e.g. Bearer) for API key, if needed
# configuration.api_key_prefix['cookieAuth'] = 'Bearer'

# Configure Bearer authorization: bearerAuth
configuration = hs5_api.Configuration(
    access_token = os.environ["BEARER_TOKEN"]
)

# Enter a context with an instance of the API client
with hs5_api.ApiClient(configuration) as api_client:
    # Create an instance of the API class
    api_instance = hs5_api.DefaultApi(api_client)
    list_user_roles_params = hs5_api.ListUserRolesParams() # ListUserRolesParams |  (optional)

    try:
        api_response = api_instance.list_user_roles(list_user_roles_params=list_user_roles_params)
        print("The response of DefaultApi->list_user_roles:\n")
        pprint(api_response)
    except Exception as e:
        print("Exception when calling DefaultApi->list_user_roles: %s\n" % e)
```



### Parameters


Name | Type | Description  | Notes
------------- | ------------- | ------------- | -------------
 **list_user_roles_params** | [**ListUserRolesParams**](ListUserRolesParams.md)|  | [optional] 

### Return type

[**ListUserRolesResp**](ListUserRolesResp.md)

### Authorization

[cookieAuth](../README.md#cookieAuth), [bearerAuth](../README.md#bearerAuth)

### HTTP request headers

 - **Content-Type**: application/json
 - **Accept**: application/json

### HTTP response details

| Status code | Description | Response headers |
|-------------|-------------|------------------|
**200** | OK |  -  |
**400** | Error |  -  |

[[Back to top]](#) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to Model list]](../README.md#documentation-for-models) [[Back to README]](../README.md)

# **list_users**
> ListUsersResp list_users(list_users_params=list_users_params)

List users

### Example

* Api Key Authentication (cookieAuth):
* Bearer Authentication (bearerAuth):

```python
import hs5_api
from hs5_api.models.list_users_params import ListUsersParams
from hs5_api.models.list_users_resp import ListUsersResp
from hs5_api.rest import ApiException
from pprint import pprint

# Defining the host is optional and defaults to http://localhost
# See configuration.py for a list of all supported configuration parameters.
configuration = hs5_api.Configuration(
    host = "http://localhost"
)

# The client must configure the authentication and authorization parameters
# in accordance with the API server security policy.
# Examples for each auth method are provided below, use the example that
# satisfies your auth use case.

# Configure API key authorization: cookieAuth
configuration.api_key['cookieAuth'] = os.environ["API_KEY"]

# Uncomment below to setup prefix (e.g. Bearer) for API key, if needed
# configuration.api_key_prefix['cookieAuth'] = 'Bearer'

# Configure Bearer authorization: bearerAuth
configuration = hs5_api.Configuration(
    access_token = os.environ["BEARER_TOKEN"]
)

# Enter a context with an instance of the API client
with hs5_api.ApiClient(configuration) as api_client:
    # Create an instance of the API class
    api_instance = hs5_api.DefaultApi(api_client)
    list_users_params = hs5_api.ListUsersParams() # ListUsersParams |  (optional)

    try:
        api_response = api_instance.list_users(list_users_params=list_users_params)
        print("The response of DefaultApi->list_users:\n")
        pprint(api_response)
    except Exception as e:
        print("Exception when calling DefaultApi->list_users: %s\n" % e)
```



### Parameters


Name | Type | Description  | Notes
------------- | ------------- | ------------- | -------------
 **list_users_params** | [**ListUsersParams**](ListUsersParams.md)|  | [optional] 

### Return type

[**ListUsersResp**](ListUsersResp.md)

### Authorization

[cookieAuth](../README.md#cookieAuth), [bearerAuth](../README.md#bearerAuth)

### HTTP request headers

 - **Content-Type**: application/json
 - **Accept**: application/json

### HTTP response details

| Status code | Description | Response headers |
|-------------|-------------|------------------|
**200** | OK |  -  |
**400** | Error |  -  |

[[Back to top]](#) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to Model list]](../README.md#documentation-for-models) [[Back to README]](../README.md)

# **login**
> LoginResp login(login_params=login_params)

Login with username and password to service

### Example

* Api Key Authentication (cookieAuth):
* Bearer Authentication (bearerAuth):

```python
import hs5_api
from hs5_api.models.login_params import LoginParams
from hs5_api.models.login_resp import LoginResp
from hs5_api.rest import ApiException
from pprint import pprint

# Defining the host is optional and defaults to http://localhost
# See configuration.py for a list of all supported configuration parameters.
configuration = hs5_api.Configuration(
    host = "http://localhost"
)

# The client must configure the authentication and authorization parameters
# in accordance with the API server security policy.
# Examples for each auth method are provided below, use the example that
# satisfies your auth use case.

# Configure API key authorization: cookieAuth
configuration.api_key['cookieAuth'] = os.environ["API_KEY"]

# Uncomment below to setup prefix (e.g. Bearer) for API key, if needed
# configuration.api_key_prefix['cookieAuth'] = 'Bearer'

# Configure Bearer authorization: bearerAuth
configuration = hs5_api.Configuration(
    access_token = os.environ["BEARER_TOKEN"]
)

# Enter a context with an instance of the API client
with hs5_api.ApiClient(configuration) as api_client:
    # Create an instance of the API class
    api_instance = hs5_api.DefaultApi(api_client)
    login_params = hs5_api.LoginParams() # LoginParams |  (optional)

    try:
        api_response = api_instance.login(login_params=login_params)
        print("The response of DefaultApi->login:\n")
        pprint(api_response)
    except Exception as e:
        print("Exception when calling DefaultApi->login: %s\n" % e)
```



### Parameters


Name | Type | Description  | Notes
------------- | ------------- | ------------- | -------------
 **login_params** | [**LoginParams**](LoginParams.md)|  | [optional] 

### Return type

[**LoginResp**](LoginResp.md)

### Authorization

[cookieAuth](../README.md#cookieAuth), [bearerAuth](../README.md#bearerAuth)

### HTTP request headers

 - **Content-Type**: application/json
 - **Accept**: application/json

### HTTP response details

| Status code | Description | Response headers |
|-------------|-------------|------------------|
**200** | OK |  -  |
**400** | Error |  -  |

[[Back to top]](#) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to Model list]](../README.md#documentation-for-models) [[Back to README]](../README.md)

# **logout**
> LogoutResp logout(logout_params=logout_params)

Logout

### Example

* Api Key Authentication (cookieAuth):
* Bearer Authentication (bearerAuth):

```python
import hs5_api
from hs5_api.models.logout_params import LogoutParams
from hs5_api.models.logout_resp import LogoutResp
from hs5_api.rest import ApiException
from pprint import pprint

# Defining the host is optional and defaults to http://localhost
# See configuration.py for a list of all supported configuration parameters.
configuration = hs5_api.Configuration(
    host = "http://localhost"
)

# The client must configure the authentication and authorization parameters
# in accordance with the API server security policy.
# Examples for each auth method are provided below, use the example that
# satisfies your auth use case.

# Configure API key authorization: cookieAuth
configuration.api_key['cookieAuth'] = os.environ["API_KEY"]

# Uncomment below to setup prefix (e.g. Bearer) for API key, if needed
# configuration.api_key_prefix['cookieAuth'] = 'Bearer'

# Configure Bearer authorization: bearerAuth
configuration = hs5_api.Configuration(
    access_token = os.environ["BEARER_TOKEN"]
)

# Enter a context with an instance of the API client
with hs5_api.ApiClient(configuration) as api_client:
    # Create an instance of the API class
    api_instance = hs5_api.DefaultApi(api_client)
    logout_params = hs5_api.LogoutParams() # LogoutParams |  (optional)

    try:
        api_response = api_instance.logout(logout_params=logout_params)
        print("The response of DefaultApi->logout:\n")
        pprint(api_response)
    except Exception as e:
        print("Exception when calling DefaultApi->logout: %s\n" % e)
```



### Parameters


Name | Type | Description  | Notes
------------- | ------------- | ------------- | -------------
 **logout_params** | [**LogoutParams**](LogoutParams.md)|  | [optional] 

### Return type

[**LogoutResp**](LogoutResp.md)

### Authorization

[cookieAuth](../README.md#cookieAuth), [bearerAuth](../README.md#bearerAuth)

### HTTP request headers

 - **Content-Type**: application/json
 - **Accept**: application/json

### HTTP response details

| Status code | Description | Response headers |
|-------------|-------------|------------------|
**200** | OK |  -  |
**400** | Error |  -  |

[[Back to top]](#) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to Model list]](../README.md#documentation-for-models) [[Back to README]](../README.md)

# **remove_access_key**
> RemoveAccessKeyResp remove_access_key(remove_access_key_params=remove_access_key_params)

Remove access key

### Example

* Api Key Authentication (cookieAuth):
* Bearer Authentication (bearerAuth):

```python
import hs5_api
from hs5_api.models.remove_access_key_params import RemoveAccessKeyParams
from hs5_api.models.remove_access_key_resp import RemoveAccessKeyResp
from hs5_api.rest import ApiException
from pprint import pprint

# Defining the host is optional and defaults to http://localhost
# See configuration.py for a list of all supported configuration parameters.
configuration = hs5_api.Configuration(
    host = "http://localhost"
)

# The client must configure the authentication and authorization parameters
# in accordance with the API server security policy.
# Examples for each auth method are provided below, use the example that
# satisfies your auth use case.

# Configure API key authorization: cookieAuth
configuration.api_key['cookieAuth'] = os.environ["API_KEY"]

# Uncomment below to setup prefix (e.g. Bearer) for API key, if needed
# configuration.api_key_prefix['cookieAuth'] = 'Bearer'

# Configure Bearer authorization: bearerAuth
configuration = hs5_api.Configuration(
    access_token = os.environ["BEARER_TOKEN"]
)

# Enter a context with an instance of the API client
with hs5_api.ApiClient(configuration) as api_client:
    # Create an instance of the API class
    api_instance = hs5_api.DefaultApi(api_client)
    remove_access_key_params = hs5_api.RemoveAccessKeyParams() # RemoveAccessKeyParams |  (optional)

    try:
        api_response = api_instance.remove_access_key(remove_access_key_params=remove_access_key_params)
        print("The response of DefaultApi->remove_access_key:\n")
        pprint(api_response)
    except Exception as e:
        print("Exception when calling DefaultApi->remove_access_key: %s\n" % e)
```



### Parameters


Name | Type | Description  | Notes
------------- | ------------- | ------------- | -------------
 **remove_access_key_params** | [**RemoveAccessKeyParams**](RemoveAccessKeyParams.md)|  | [optional] 

### Return type

[**RemoveAccessKeyResp**](RemoveAccessKeyResp.md)

### Authorization

[cookieAuth](../README.md#cookieAuth), [bearerAuth](../README.md#bearerAuth)

### HTTP request headers

 - **Content-Type**: application/json
 - **Accept**: application/json

### HTTP response details

| Status code | Description | Response headers |
|-------------|-------------|------------------|
**200** | OK |  -  |
**400** | Error |  -  |

[[Back to top]](#) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to Model list]](../README.md#documentation-for-models) [[Back to README]](../README.md)

# **remove_policy**
> RemovePolicyResp remove_policy(remove_policy_params=remove_policy_params)

Remove policy

### Example

* Api Key Authentication (cookieAuth):
* Bearer Authentication (bearerAuth):

```python
import hs5_api
from hs5_api.models.remove_policy_params import RemovePolicyParams
from hs5_api.models.remove_policy_resp import RemovePolicyResp
from hs5_api.rest import ApiException
from pprint import pprint

# Defining the host is optional and defaults to http://localhost
# See configuration.py for a list of all supported configuration parameters.
configuration = hs5_api.Configuration(
    host = "http://localhost"
)

# The client must configure the authentication and authorization parameters
# in accordance with the API server security policy.
# Examples for each auth method are provided below, use the example that
# satisfies your auth use case.

# Configure API key authorization: cookieAuth
configuration.api_key['cookieAuth'] = os.environ["API_KEY"]

# Uncomment below to setup prefix (e.g. Bearer) for API key, if needed
# configuration.api_key_prefix['cookieAuth'] = 'Bearer'

# Configure Bearer authorization: bearerAuth
configuration = hs5_api.Configuration(
    access_token = os.environ["BEARER_TOKEN"]
)

# Enter a context with an instance of the API client
with hs5_api.ApiClient(configuration) as api_client:
    # Create an instance of the API class
    api_instance = hs5_api.DefaultApi(api_client)
    remove_policy_params = hs5_api.RemovePolicyParams() # RemovePolicyParams |  (optional)

    try:
        api_response = api_instance.remove_policy(remove_policy_params=remove_policy_params)
        print("The response of DefaultApi->remove_policy:\n")
        pprint(api_response)
    except Exception as e:
        print("Exception when calling DefaultApi->remove_policy: %s\n" % e)
```



### Parameters


Name | Type | Description  | Notes
------------- | ------------- | ------------- | -------------
 **remove_policy_params** | [**RemovePolicyParams**](RemovePolicyParams.md)|  | [optional] 

### Return type

[**RemovePolicyResp**](RemovePolicyResp.md)

### Authorization

[cookieAuth](../README.md#cookieAuth), [bearerAuth](../README.md#bearerAuth)

### HTTP request headers

 - **Content-Type**: application/json
 - **Accept**: application/json

### HTTP response details

| Status code | Description | Response headers |
|-------------|-------------|------------------|
**200** | OK |  -  |
**400** | Error |  -  |

[[Back to top]](#) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to Model list]](../README.md#documentation-for-models) [[Back to README]](../README.md)

# **remove_role**
> RemoveRoleResp remove_role(remove_role_params=remove_role_params)

Remove role

### Example

* Api Key Authentication (cookieAuth):
* Bearer Authentication (bearerAuth):

```python
import hs5_api
from hs5_api.models.remove_role_params import RemoveRoleParams
from hs5_api.models.remove_role_resp import RemoveRoleResp
from hs5_api.rest import ApiException
from pprint import pprint

# Defining the host is optional and defaults to http://localhost
# See configuration.py for a list of all supported configuration parameters.
configuration = hs5_api.Configuration(
    host = "http://localhost"
)

# The client must configure the authentication and authorization parameters
# in accordance with the API server security policy.
# Examples for each auth method are provided below, use the example that
# satisfies your auth use case.

# Configure API key authorization: cookieAuth
configuration.api_key['cookieAuth'] = os.environ["API_KEY"]

# Uncomment below to setup prefix (e.g. Bearer) for API key, if needed
# configuration.api_key_prefix['cookieAuth'] = 'Bearer'

# Configure Bearer authorization: bearerAuth
configuration = hs5_api.Configuration(
    access_token = os.environ["BEARER_TOKEN"]
)

# Enter a context with an instance of the API client
with hs5_api.ApiClient(configuration) as api_client:
    # Create an instance of the API class
    api_instance = hs5_api.DefaultApi(api_client)
    remove_role_params = hs5_api.RemoveRoleParams() # RemoveRoleParams |  (optional)

    try:
        api_response = api_instance.remove_role(remove_role_params=remove_role_params)
        print("The response of DefaultApi->remove_role:\n")
        pprint(api_response)
    except Exception as e:
        print("Exception when calling DefaultApi->remove_role: %s\n" % e)
```



### Parameters


Name | Type | Description  | Notes
------------- | ------------- | ------------- | -------------
 **remove_role_params** | [**RemoveRoleParams**](RemoveRoleParams.md)|  | [optional] 

### Return type

[**RemoveRoleResp**](RemoveRoleResp.md)

### Authorization

[cookieAuth](../README.md#cookieAuth), [bearerAuth](../README.md#bearerAuth)

### HTTP request headers

 - **Content-Type**: application/json
 - **Accept**: application/json

### HTTP response details

| Status code | Description | Response headers |
|-------------|-------------|------------------|
**200** | OK |  -  |
**400** | Error |  -  |

[[Back to top]](#) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to Model list]](../README.md#documentation-for-models) [[Back to README]](../README.md)

# **remove_role_policy**
> RemoveRolePolicyResp remove_role_policy(remove_role_policy_params=remove_role_policy_params)

Remove role policy

### Example

* Api Key Authentication (cookieAuth):
* Bearer Authentication (bearerAuth):

```python
import hs5_api
from hs5_api.models.remove_role_policy_params import RemoveRolePolicyParams
from hs5_api.models.remove_role_policy_resp import RemoveRolePolicyResp
from hs5_api.rest import ApiException
from pprint import pprint

# Defining the host is optional and defaults to http://localhost
# See configuration.py for a list of all supported configuration parameters.
configuration = hs5_api.Configuration(
    host = "http://localhost"
)

# The client must configure the authentication and authorization parameters
# in accordance with the API server security policy.
# Examples for each auth method are provided below, use the example that
# satisfies your auth use case.

# Configure API key authorization: cookieAuth
configuration.api_key['cookieAuth'] = os.environ["API_KEY"]

# Uncomment below to setup prefix (e.g. Bearer) for API key, if needed
# configuration.api_key_prefix['cookieAuth'] = 'Bearer'

# Configure Bearer authorization: bearerAuth
configuration = hs5_api.Configuration(
    access_token = os.environ["BEARER_TOKEN"]
)

# Enter a context with an instance of the API client
with hs5_api.ApiClient(configuration) as api_client:
    # Create an instance of the API class
    api_instance = hs5_api.DefaultApi(api_client)
    remove_role_policy_params = hs5_api.RemoveRolePolicyParams() # RemoveRolePolicyParams |  (optional)

    try:
        api_response = api_instance.remove_role_policy(remove_role_policy_params=remove_role_policy_params)
        print("The response of DefaultApi->remove_role_policy:\n")
        pprint(api_response)
    except Exception as e:
        print("Exception when calling DefaultApi->remove_role_policy: %s\n" % e)
```



### Parameters


Name | Type | Description  | Notes
------------- | ------------- | ------------- | -------------
 **remove_role_policy_params** | [**RemoveRolePolicyParams**](RemoveRolePolicyParams.md)|  | [optional] 

### Return type

[**RemoveRolePolicyResp**](RemoveRolePolicyResp.md)

### Authorization

[cookieAuth](../README.md#cookieAuth), [bearerAuth](../README.md#bearerAuth)

### HTTP request headers

 - **Content-Type**: application/json
 - **Accept**: application/json

### HTTP response details

| Status code | Description | Response headers |
|-------------|-------------|------------------|
**200** | OK |  -  |
**400** | Error |  -  |

[[Back to top]](#) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to Model list]](../README.md#documentation-for-models) [[Back to README]](../README.md)

# **remove_user**
> RemoveUserResp remove_user(remove_user_params=remove_user_params)

Remove a user

### Example

* Api Key Authentication (cookieAuth):
* Bearer Authentication (bearerAuth):

```python
import hs5_api
from hs5_api.models.remove_user_params import RemoveUserParams
from hs5_api.models.remove_user_resp import RemoveUserResp
from hs5_api.rest import ApiException
from pprint import pprint

# Defining the host is optional and defaults to http://localhost
# See configuration.py for a list of all supported configuration parameters.
configuration = hs5_api.Configuration(
    host = "http://localhost"
)

# The client must configure the authentication and authorization parameters
# in accordance with the API server security policy.
# Examples for each auth method are provided below, use the example that
# satisfies your auth use case.

# Configure API key authorization: cookieAuth
configuration.api_key['cookieAuth'] = os.environ["API_KEY"]

# Uncomment below to setup prefix (e.g. Bearer) for API key, if needed
# configuration.api_key_prefix['cookieAuth'] = 'Bearer'

# Configure Bearer authorization: bearerAuth
configuration = hs5_api.Configuration(
    access_token = os.environ["BEARER_TOKEN"]
)

# Enter a context with an instance of the API client
with hs5_api.ApiClient(configuration) as api_client:
    # Create an instance of the API class
    api_instance = hs5_api.DefaultApi(api_client)
    remove_user_params = hs5_api.RemoveUserParams() # RemoveUserParams |  (optional)

    try:
        api_response = api_instance.remove_user(remove_user_params=remove_user_params)
        print("The response of DefaultApi->remove_user:\n")
        pprint(api_response)
    except Exception as e:
        print("Exception when calling DefaultApi->remove_user: %s\n" % e)
```



### Parameters


Name | Type | Description  | Notes
------------- | ------------- | ------------- | -------------
 **remove_user_params** | [**RemoveUserParams**](RemoveUserParams.md)|  | [optional] 

### Return type

[**RemoveUserResp**](RemoveUserResp.md)

### Authorization

[cookieAuth](../README.md#cookieAuth), [bearerAuth](../README.md#bearerAuth)

### HTTP request headers

 - **Content-Type**: application/json
 - **Accept**: application/json

### HTTP response details

| Status code | Description | Response headers |
|-------------|-------------|------------------|
**200** | OK |  -  |
**400** | Error |  -  |

[[Back to top]](#) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to Model list]](../README.md#documentation-for-models) [[Back to README]](../README.md)

# **remove_user_role**
> RemoveUserRoleResp remove_user_role(remove_user_role_params=remove_user_role_params)

Remove user role

### Example

* Api Key Authentication (cookieAuth):
* Bearer Authentication (bearerAuth):

```python
import hs5_api
from hs5_api.models.remove_user_role_params import RemoveUserRoleParams
from hs5_api.models.remove_user_role_resp import RemoveUserRoleResp
from hs5_api.rest import ApiException
from pprint import pprint

# Defining the host is optional and defaults to http://localhost
# See configuration.py for a list of all supported configuration parameters.
configuration = hs5_api.Configuration(
    host = "http://localhost"
)

# The client must configure the authentication and authorization parameters
# in accordance with the API server security policy.
# Examples for each auth method are provided below, use the example that
# satisfies your auth use case.

# Configure API key authorization: cookieAuth
configuration.api_key['cookieAuth'] = os.environ["API_KEY"]

# Uncomment below to setup prefix (e.g. Bearer) for API key, if needed
# configuration.api_key_prefix['cookieAuth'] = 'Bearer'

# Configure Bearer authorization: bearerAuth
configuration = hs5_api.Configuration(
    access_token = os.environ["BEARER_TOKEN"]
)

# Enter a context with an instance of the API client
with hs5_api.ApiClient(configuration) as api_client:
    # Create an instance of the API class
    api_instance = hs5_api.DefaultApi(api_client)
    remove_user_role_params = hs5_api.RemoveUserRoleParams() # RemoveUserRoleParams |  (optional)

    try:
        api_response = api_instance.remove_user_role(remove_user_role_params=remove_user_role_params)
        print("The response of DefaultApi->remove_user_role:\n")
        pprint(api_response)
    except Exception as e:
        print("Exception when calling DefaultApi->remove_user_role: %s\n" % e)
```



### Parameters


Name | Type | Description  | Notes
------------- | ------------- | ------------- | -------------
 **remove_user_role_params** | [**RemoveUserRoleParams**](RemoveUserRoleParams.md)|  | [optional] 

### Return type

[**RemoveUserRoleResp**](RemoveUserRoleResp.md)

### Authorization

[cookieAuth](../README.md#cookieAuth), [bearerAuth](../README.md#bearerAuth)

### HTTP request headers

 - **Content-Type**: application/json
 - **Accept**: application/json

### HTTP response details

| Status code | Description | Response headers |
|-------------|-------------|------------------|
**200** | OK |  -  |
**400** | Error |  -  |

[[Back to top]](#) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to Model list]](../README.md#documentation-for-models) [[Back to README]](../README.md)

# **session_check**
> SessionCheckResp session_check(session_check_params=session_check_params)

Check if session is ok

### Example

* Api Key Authentication (cookieAuth):
* Bearer Authentication (bearerAuth):

```python
import hs5_api
from hs5_api.models.session_check_params import SessionCheckParams
from hs5_api.models.session_check_resp import SessionCheckResp
from hs5_api.rest import ApiException
from pprint import pprint

# Defining the host is optional and defaults to http://localhost
# See configuration.py for a list of all supported configuration parameters.
configuration = hs5_api.Configuration(
    host = "http://localhost"
)

# The client must configure the authentication and authorization parameters
# in accordance with the API server security policy.
# Examples for each auth method are provided below, use the example that
# satisfies your auth use case.

# Configure API key authorization: cookieAuth
configuration.api_key['cookieAuth'] = os.environ["API_KEY"]

# Uncomment below to setup prefix (e.g. Bearer) for API key, if needed
# configuration.api_key_prefix['cookieAuth'] = 'Bearer'

# Configure Bearer authorization: bearerAuth
configuration = hs5_api.Configuration(
    access_token = os.environ["BEARER_TOKEN"]
)

# Enter a context with an instance of the API client
with hs5_api.ApiClient(configuration) as api_client:
    # Create an instance of the API class
    api_instance = hs5_api.DefaultApi(api_client)
    session_check_params = hs5_api.SessionCheckParams() # SessionCheckParams |  (optional)

    try:
        api_response = api_instance.session_check(session_check_params=session_check_params)
        print("The response of DefaultApi->session_check:\n")
        pprint(api_response)
    except Exception as e:
        print("Exception when calling DefaultApi->session_check: %s\n" % e)
```



### Parameters


Name | Type | Description  | Notes
------------- | ------------- | ------------- | -------------
 **session_check_params** | [**SessionCheckParams**](SessionCheckParams.md)|  | [optional] 

### Return type

[**SessionCheckResp**](SessionCheckResp.md)

### Authorization

[cookieAuth](../README.md#cookieAuth), [bearerAuth](../README.md#bearerAuth)

### HTTP request headers

 - **Content-Type**: application/json
 - **Accept**: application/json

### HTTP response details

| Status code | Description | Response headers |
|-------------|-------------|------------------|
**200** | OK |  -  |
**400** | Error |  -  |

[[Back to top]](#) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to Model list]](../README.md#documentation-for-models) [[Back to README]](../README.md)

