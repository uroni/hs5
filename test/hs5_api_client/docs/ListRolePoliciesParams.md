# ListRolePoliciesParams


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**role_id** | **str** |  | 
**continuation_token** | **str** |  | [optional] 

## Example

```python
from hs5_api.models.list_role_policies_params import ListRolePoliciesParams

# TODO update the JSON string below
json = "{}"
# create an instance of ListRolePoliciesParams from a JSON string
list_role_policies_params_instance = ListRolePoliciesParams.from_json(json)
# print the JSON string representation of the object
print(ListRolePoliciesParams.to_json())

# convert the object into a dict
list_role_policies_params_dict = list_role_policies_params_instance.to_dict()
# create an instance of ListRolePoliciesParams from a dict
list_role_policies_params_from_dict = ListRolePoliciesParams.from_dict(list_role_policies_params_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


