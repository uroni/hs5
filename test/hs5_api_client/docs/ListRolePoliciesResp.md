# ListRolePoliciesResp


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**role_policies** | [**List[ListRolePoliciesRespRolePoliciesInner]**](ListRolePoliciesRespRolePoliciesInner.md) |  | 
**next_marker** | **str** |  | 
**is_truncated** | **bool** |  | 

## Example

```python
from hs5_api.models.list_role_policies_resp import ListRolePoliciesResp

# TODO update the JSON string below
json = "{}"
# create an instance of ListRolePoliciesResp from a JSON string
list_role_policies_resp_instance = ListRolePoliciesResp.from_json(json)
# print the JSON string representation of the object
print(ListRolePoliciesResp.to_json())

# convert the object into a dict
list_role_policies_resp_dict = list_role_policies_resp_instance.to_dict()
# create an instance of ListRolePoliciesResp from a dict
list_role_policies_resp_from_dict = ListRolePoliciesResp.from_dict(list_role_policies_resp_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


