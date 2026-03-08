# ListRolesResp


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**roles** | [**List[ListRolesRespRolesInner]**](ListRolesRespRolesInner.md) |  | 
**next_marker** | **str** |  | 
**is_truncated** | **bool** |  | 

## Example

```python
from hs5_api.models.list_roles_resp import ListRolesResp

# TODO update the JSON string below
json = "{}"
# create an instance of ListRolesResp from a JSON string
list_roles_resp_instance = ListRolesResp.from_json(json)
# print the JSON string representation of the object
print(ListRolesResp.to_json())

# convert the object into a dict
list_roles_resp_dict = list_roles_resp_instance.to_dict()
# create an instance of ListRolesResp from a dict
list_roles_resp_from_dict = ListRolesResp.from_dict(list_roles_resp_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


