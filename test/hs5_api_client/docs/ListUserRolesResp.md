# ListUserRolesResp


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**user_roles** | [**List[ListUserRolesRespUserRolesInner]**](ListUserRolesRespUserRolesInner.md) |  | 
**next_marker** | **str** |  | 
**is_truncated** | **bool** |  | 

## Example

```python
from hs5_api.models.list_user_roles_resp import ListUserRolesResp

# TODO update the JSON string below
json = "{}"
# create an instance of ListUserRolesResp from a JSON string
list_user_roles_resp_instance = ListUserRolesResp.from_json(json)
# print the JSON string representation of the object
print(ListUserRolesResp.to_json())

# convert the object into a dict
list_user_roles_resp_dict = list_user_roles_resp_instance.to_dict()
# create an instance of ListUserRolesResp from a dict
list_user_roles_resp_from_dict = ListUserRolesResp.from_dict(list_user_roles_resp_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


