# ListUserRolesRespUserRolesInner


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**id** | **str** |  | 
**role_id** | **str** |  | 
**role_name** | **str** |  | 
**system** | **bool** |  | 

## Example

```python
from hs5_api.models.list_user_roles_resp_user_roles_inner import ListUserRolesRespUserRolesInner

# TODO update the JSON string below
json = "{}"
# create an instance of ListUserRolesRespUserRolesInner from a JSON string
list_user_roles_resp_user_roles_inner_instance = ListUserRolesRespUserRolesInner.from_json(json)
# print the JSON string representation of the object
print(ListUserRolesRespUserRolesInner.to_json())

# convert the object into a dict
list_user_roles_resp_user_roles_inner_dict = list_user_roles_resp_user_roles_inner_instance.to_dict()
# create an instance of ListUserRolesRespUserRolesInner from a dict
list_user_roles_resp_user_roles_inner_from_dict = ListUserRolesRespUserRolesInner.from_dict(list_user_roles_resp_user_roles_inner_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


