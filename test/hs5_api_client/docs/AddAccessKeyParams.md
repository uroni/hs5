# AddAccessKeyParams


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**ses** | **str** |  | 
**user_id** | **str** |  | 

## Example

```python
from hs5_api.models.add_access_key_params import AddAccessKeyParams

# TODO update the JSON string below
json = "{}"
# create an instance of AddAccessKeyParams from a JSON string
add_access_key_params_instance = AddAccessKeyParams.from_json(json)
# print the JSON string representation of the object
print(AddAccessKeyParams.to_json())

# convert the object into a dict
add_access_key_params_dict = add_access_key_params_instance.to_dict()
# create an instance of AddAccessKeyParams from a dict
add_access_key_params_from_dict = AddAccessKeyParams.from_dict(add_access_key_params_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


