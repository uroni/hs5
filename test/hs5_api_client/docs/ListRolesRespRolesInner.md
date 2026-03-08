# ListRolesRespRolesInner


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**id** | **str** |  | 
**role_name** | **str** |  | 
**system** | **bool** |  | 

## Example

```python
from hs5_api.models.list_roles_resp_roles_inner import ListRolesRespRolesInner

# TODO update the JSON string below
json = "{}"
# create an instance of ListRolesRespRolesInner from a JSON string
list_roles_resp_roles_inner_instance = ListRolesRespRolesInner.from_json(json)
# print the JSON string representation of the object
print(ListRolesRespRolesInner.to_json())

# convert the object into a dict
list_roles_resp_roles_inner_dict = list_roles_resp_roles_inner_instance.to_dict()
# create an instance of ListRolesRespRolesInner from a dict
list_roles_resp_roles_inner_from_dict = ListRolesRespRolesInner.from_dict(list_roles_resp_roles_inner_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


