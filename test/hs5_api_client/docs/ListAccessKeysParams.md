# ListAccessKeysParams


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**ses** | **str** |  | 
**user_id** | **str** |  | 
**continuation_token** | **str** |  | [optional] 

## Example

```python
from hs5_api.models.list_access_keys_params import ListAccessKeysParams

# TODO update the JSON string below
json = "{}"
# create an instance of ListAccessKeysParams from a JSON string
list_access_keys_params_instance = ListAccessKeysParams.from_json(json)
# print the JSON string representation of the object
print(ListAccessKeysParams.to_json())

# convert the object into a dict
list_access_keys_params_dict = list_access_keys_params_instance.to_dict()
# create an instance of ListAccessKeysParams from a dict
list_access_keys_params_from_dict = ListAccessKeysParams.from_dict(list_access_keys_params_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


