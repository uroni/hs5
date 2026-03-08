# ListBucketPermissionsResp


## Properties

Name | Type | Description | Notes
------------ | ------------- | ------------- | -------------
**bucket_permissions** | [**List[ListBucketPermissionsRespBucketPermissionsInner]**](ListBucketPermissionsRespBucketPermissionsInner.md) |  | 
**next_marker** | **str** |  | 
**is_truncated** | **bool** |  | 

## Example

```python
from hs5_api.models.list_bucket_permissions_resp import ListBucketPermissionsResp

# TODO update the JSON string below
json = "{}"
# create an instance of ListBucketPermissionsResp from a JSON string
list_bucket_permissions_resp_instance = ListBucketPermissionsResp.from_json(json)
# print the JSON string representation of the object
print(ListBucketPermissionsResp.to_json())

# convert the object into a dict
list_bucket_permissions_resp_dict = list_bucket_permissions_resp_instance.to_dict()
# create an instance of ListBucketPermissionsResp from a dict
list_bucket_permissions_resp_from_dict = ListBucketPermissionsResp.from_dict(list_bucket_permissions_resp_dict)
```
[[Back to Model list]](../README.md#documentation-for-models) [[Back to API list]](../README.md#documentation-for-api-endpoints) [[Back to README]](../README.md)


