# ChangePolicyParams


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**ses** | **str** |  | 
**id** | **str** |  | 
**document** | **str** |  | 

## Example

```python
from hs5_api.models.change_policy_params import ChangePolicyParams

# TODO update the JSON string below
json = "{}"
# create an instance of ChangePolicyParams from a JSON string
change_policy_params_instance = ChangePolicyParams.from_json(json)
# print the JSON string representation of the object
print(ChangePolicyParams.to_json())

# convert the object into a dict
change_policy_params_dict = change_policy_params_instance.to_dict()
# create an instance of ChangePolicyParams from a dict
change_policy_params_from_dict = ChangePolicyParams.from_dict(change_policy_params_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


