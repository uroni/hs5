# ListUsersResp


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**users** | [**List[ListUsersRespUsersInner]**](ListUsersRespUsersInner.md) |  | 
**next_marker** | **str** |  | 
**is_truncated** | **bool** |  | 

## Example

```python
from hs5_api.models.list_users_resp import ListUsersResp

# TODO update the JSON string below
json = "{}"
# create an instance of ListUsersResp from a JSON string
list_users_resp_instance = ListUsersResp.from_json(json)
# print the JSON string representation of the object
print(ListUsersResp.to_json())

# convert the object into a dict
list_users_resp_dict = list_users_resp_instance.to_dict()
# create an instance of ListUsersResp from a dict
list_users_resp_from_dict = ListUsersResp.from_dict(list_users_resp_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


