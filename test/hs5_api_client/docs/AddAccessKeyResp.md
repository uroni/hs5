# AddAccessKeyResp


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**id** | **str** |  | 
**access_key** | **str** |  | 
**secret_key** | **str** |  | 

## Example

```python
from hs5_api.models.add_access_key_resp import AddAccessKeyResp

# TODO update the JSON string below
json = "{}"
# create an instance of AddAccessKeyResp from a JSON string
add_access_key_resp_instance = AddAccessKeyResp.from_json(json)
# print the JSON string representation of the object
print(AddAccessKeyResp.to_json())

# convert the object into a dict
add_access_key_resp_dict = add_access_key_resp_instance.to_dict()
# create an instance of AddAccessKeyResp from a dict
add_access_key_resp_from_dict = AddAccessKeyResp.from_dict(add_access_key_resp_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


