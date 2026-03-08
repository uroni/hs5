# AddPolicyParams


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**ses** | **str** |  | 
**policy_name** | **str** |  | 
**policy_document** | **str** |  | 

## Example

```python
from hs5_api.models.add_policy_params import AddPolicyParams

# TODO update the JSON string below
json = "{}"
# create an instance of AddPolicyParams from a JSON string
add_policy_params_instance = AddPolicyParams.from_json(json)
# print the JSON string representation of the object
print(AddPolicyParams.to_json())

# convert the object into a dict
add_policy_params_dict = add_policy_params_instance.to_dict()
# create an instance of AddPolicyParams from a dict
add_policy_params_from_dict = AddPolicyParams.from_dict(add_policy_params_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


