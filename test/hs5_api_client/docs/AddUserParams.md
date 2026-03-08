# AddUserParams


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**ses** | **str** |  | 
**username** | **str** |  | 
**password** | **str** |  | 

## Example

```python
from hs5_api.models.add_user_params import AddUserParams

# TODO update the JSON string below
json = "{}"
# create an instance of AddUserParams from a JSON string
add_user_params_instance = AddUserParams.from_json(json)
# print the JSON string representation of the object
print(AddUserParams.to_json())

# convert the object into a dict
add_user_params_dict = add_user_params_instance.to_dict()
# create an instance of AddUserParams from a dict
add_user_params_from_dict = AddUserParams.from_dict(add_user_params_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


