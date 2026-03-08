# AddRolePolicyParams


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**role_id** | **str** |  | 
**policy_id** | **str** |  | 

## Example

```python
from hs5_api.models.add_role_policy_params import AddRolePolicyParams

# TODO update the JSON string below
json = "{}"
# create an instance of AddRolePolicyParams from a JSON string
add_role_policy_params_instance = AddRolePolicyParams.from_json(json)
# print the JSON string representation of the object
print(AddRolePolicyParams.to_json())

# convert the object into a dict
add_role_policy_params_dict = add_role_policy_params_instance.to_dict()
# create an instance of AddRolePolicyParams from a dict
add_role_policy_params_from_dict = AddRolePolicyParams.from_dict(add_role_policy_params_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


