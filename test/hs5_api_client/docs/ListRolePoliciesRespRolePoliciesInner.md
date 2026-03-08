# ListRolePoliciesRespRolePoliciesInner


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**id** | **str** |  | 
**policy_id** | **str** |  | 
**policy_name** | **str** |  | 
**system** | **bool** |  | 

## Example

```python
from hs5_api.models.list_role_policies_resp_role_policies_inner import ListRolePoliciesRespRolePoliciesInner

# TODO update the JSON string below
json = "{}"
# create an instance of ListRolePoliciesRespRolePoliciesInner from a JSON string
list_role_policies_resp_role_policies_inner_instance = ListRolePoliciesRespRolePoliciesInner.from_json(json)
# print the JSON string representation of the object
print(ListRolePoliciesRespRolePoliciesInner.to_json())

# convert the object into a dict
list_role_policies_resp_role_policies_inner_dict = list_role_policies_resp_role_policies_inner_instance.to_dict()
# create an instance of ListRolePoliciesRespRolePoliciesInner from a dict
list_role_policies_resp_role_policies_inner_from_dict = ListRolePoliciesRespRolePoliciesInner.from_dict(list_role_policies_resp_role_policies_inner_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


