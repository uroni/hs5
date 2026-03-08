# RemoveUserParams


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**ses** | **str** |  | 
**username** | **str** |  | 

## Example

```python
from hs5_api.models.remove_user_params import RemoveUserParams

# TODO update the JSON string below
json = "{}"
# create an instance of RemoveUserParams from a JSON string
remove_user_params_instance = RemoveUserParams.from_json(json)
# print the JSON string representation of the object
print(RemoveUserParams.to_json())

# convert the object into a dict
remove_user_params_dict = remove_user_params_instance.to_dict()
# create an instance of RemoveUserParams from a dict
remove_user_params_from_dict = RemoveUserParams.from_dict(remove_user_params_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


