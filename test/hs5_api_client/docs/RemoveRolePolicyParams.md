# RemoveRolePolicyParams


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**ses** | **str** |  | 
**id** | **str** |  | 

## Example

```python
from hs5_api.models.remove_role_policy_params import RemoveRolePolicyParams

# TODO update the JSON string below
json = "{}"
# create an instance of RemoveRolePolicyParams from a JSON string
remove_role_policy_params_instance = RemoveRolePolicyParams.from_json(json)
# print the JSON string representation of the object
print(RemoveRolePolicyParams.to_json())

# convert the object into a dict
remove_role_policy_params_dict = remove_role_policy_params_instance.to_dict()
# create an instance of RemoveRolePolicyParams from a dict
remove_role_policy_params_from_dict = RemoveRolePolicyParams.from_dict(remove_role_policy_params_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


